// inclusion guard
#ifndef _UPDATER_HPMC_CLUSTERS_
#define _UPDATER_HPMC_CLUSTERS_

/*! \file UpdaterBoxClusters.h
    \brief Declaration of UpdaterBoxClusters
*/

#include "hoomd/Updater.h"
#include "hoomd/Saru.h"

#include <set>
#include <list>

#include "Moves.h"
#include "HPMCCounters.h"
#include "IntegratorHPMCMono.h"

namespace hpmc
{

namespace detail
{

// Graph class represents a undirected graph
// using adjacency list representation
class Graph
    {
    int V;    // No. of vertices

    // Pointer to an array containing adjacency lists
    std::vector<std::list<int> > adj;

    // A function used by DFS
    inline void DFSUtil(int v, std::vector<bool>& visited, std::vector<unsigned int>& cur_cc);

public:
    Graph()         //!< Default constructor
        : V(0) {}

    inline Graph(int V);   // Constructor
    inline void addEdge(int v, int w);
    inline void connectedComponents(std::vector<std::vector<unsigned int> >& cc);
    };

// Gather connected components in an undirected graph
void Graph::connectedComponents(std::vector<std::vector<unsigned int> >& cc)
    {
    // Mark all the vertices as not visited
    std::vector<bool> visited(V,false);
    for (int v=0; v<V; v++)
        {
        if (visited[v] == false)
            {
            std::vector<unsigned int> cur_cc;
            DFSUtil(v, visited, cur_cc);
            cc.push_back(cur_cc);
            }
        }
    }

void Graph::DFSUtil(int v, std::vector<bool>& visited, std::vector<unsigned int>& cur_cc)
    {
    visited[v] = true;
    cur_cc.push_back(v);

    // Recur for all the vertices
    // adjacent to this vertex
    for(auto i = adj[v].begin(); i != adj[v].end(); ++i)
        {
        if(!visited[*i])
            DFSUtil(*i, visited, cur_cc);
        }
    }

Graph::Graph(int V)
    {
    this->V = V;
    adj.resize(V);
    }

// method to add an undirected edge
void Graph::addEdge(int v, int w)
    {
    adj[v].push_back(w);
    adj[w].push_back(v);
    }
} // end namespace detail

/*! A generic cluster move for attractive interactions.

    The cluster move set employed consists of pivot (point mirroring) and
    line reflection (pi rotation) moves. The algorithm therefore implements
    a simplified version of the Geometric Cluster algorithm, cf. Liu and Luijten
    PRL 2004 and Sinkovits, Barr and Luijten JCP 2012.

    In order to support anisotropic particles, we have to reject moves that
    cross the PBC, as described in Sinkovits et al.
*/

template< class Shape >
class UpdaterClusters : public Updater
    {
    public:
        //! Constructor
        /*! \param sysdef System definition
            \param mc HPMC integrator
            \param seed PRNG seed
        */
        UpdaterClusters(std::shared_ptr<SystemDefinition> sysdef,
                        std::shared_ptr<IntegratorHPMCMono<Shape> > mc,
                        unsigned int seed);

        //! Destructor
        virtual ~UpdaterClusters();

        //! Get the value of a logged quantity
        virtual Scalar getLogValue(const std::string& quantity, unsigned int timestep)
            {
            hpmc_counters_t counters = getCounters(2);

            if (quantity == "hpmc_cluster_moves")
                {
                hpmc_counters_t counters_total = getCounters(0);
                return double(counters_total.getNMoves()) / double(m_pdata->getNGlobal());
                }
            else if (quantity == "hpmc_cluster_pivot_acceptance")
                {
                return counters.getTranslateAcceptance();
                }
            else if (quantity == "hpmc_cluster_reflection_acceptance")
                {
                return counters.getRotateAcceptance();
                }
            return Scalar(0.0);
            }

        /*
            \returns a list of provided quantities
        */
        std::vector< std::string > getProvidedLogQuantities()
            {
            // start with the integrator provided quantities
            std::vector< std::string > result;
            // then add ours
            result.push_back("hpmc_cluster_moves");
            result.push_back("hpmc_cluster_pivot_acceptance");
            result.push_back("hpmc_cluster_reflection_acceptance");
            return result;
            }

        //! Take one timestep forward
        /*! \param timestep timestep at which update is being evaluated
        */
        virtual void update(unsigned int timestep);

        //! Set the move ratio
        void setMoveRatio(Scalar move_ratio)
            {
            m_move_ratio = move_ratio;
            }

        //! Reset statistics counters
        virtual void resetStats()
            {
            m_count_run_start = m_count_total;
            }

        //! Print statistics about the cluster move updates
        /* We only print the statistics about accepted and rejected moves.
         */
        void printStats()
            {
            hpmc_counters_t counters = getCounters(1);
            m_exec_conf->msg->notice(2) << "-- HPMC cluster move stats:" << std::endl;
            if (counters.translate_accept_count + counters.translate_reject_count != 0)
                {
                m_exec_conf->msg->notice(2) << "Average pivot acceptance: " << counters.getTranslateAcceptance() << std::endl;
                }
            if (counters.rotate_accept_count + counters.rotate_reject_count != 0)
                {
                m_exec_conf->msg->notice(2) << "Average reflection acceptance:    " << counters.getRotateAcceptance() << std::endl;
                }

            m_exec_conf->msg->notice(2) << "Total cluster moves:          " << counters.getNMoves() << std::endl;
            }

            /*! \param mode 0 -> Absolute count, 1 -> relative to the start of the run, 2 -> relative to the last executed step
                \return The current state of the acceptance counters
            */
            hpmc_counters_t getCounters(unsigned int mode)
                {
                hpmc_counters_t result;

                if (mode == 0)
                    result = m_count_total;
                else if (mode == 1)
                    result = m_count_total - m_count_run_start;
                else
                    result = m_count_total - m_count_step_start;

                #ifdef ENABLE_MPI
                if (m_pdata->getDomainDecomposition())
                    {
                    bcast(result.translate_accept_count,0,m_exec_conf->getMPICommunicator());
                    bcast(result.rotate_accept_count,0,m_exec_conf->getMPICommunicator());
                    bcast(result.translate_reject_count,0,m_exec_conf->getMPICommunicator());
                    bcast(result.rotate_reject_count,0,m_exec_conf->getMPICommunicator());
                    }
                #endif

                return result;
                }


    protected:
        std::shared_ptr< IntegratorHPMCMono<Shape> > m_mc; //!< HPMC integrator
        unsigned int m_seed;                        //!< RNG seed
        Scalar m_move_ratio;                        //!< Pivot/Reflection move ratio

        std::vector<std::vector<unsigned int> > m_clusters; //!< Cluster components

        detail::Graph m_G; //!< The graph

        unsigned int m_n_particles_old;                //!< Number of local particles in the old configuration
        detail::AABBTree m_aabb_tree_old;              //!< Locality lookup for old configuration
        std::vector<Scalar4> m_postype_backup;         //!< Old local positions
        std::vector<Scalar4> m_orientation_backup;     //!< Old local orientations
        std::vector<Scalar> m_diameter_backup;         //!< Old local diameters
        std::vector<Scalar> m_charge_backup;           //!< Old local charges
        std::vector<int3> m_image_backup;              //!< Old local images

        std::vector<unsigned int> m_tag_backup;             //!< Old local tags

        std::set<std::pair<unsigned int, unsigned int> > m_overlap;   //!< A local set of particle pairs due to overlap
        std::set<std::pair<unsigned int, unsigned int> > m_interact_old_old;  //!< Pairs interacting old-old
        std::set<std::pair<unsigned int, unsigned int> > m_interact_new_old;  //!< Pairs interacting new-old
        std::set<std::pair<unsigned int, unsigned int> > m_interact_new_new;  //!< Pairs interacting new-old
        std::set<unsigned int> m_local_reject;                   //!< Set of particles whose clusters moves are rejected

        std::map<std::pair<unsigned int, unsigned int>,float > m_energy_old_old;    //!< Energy of interaction old-old
        std::map<std::pair<unsigned int, unsigned int>,float > m_energy_new_old;    //!< Energy of interaction old-old

        std::set<unsigned int> m_ptl_reject;              //!< List of ptls that are not transformed
        hpmc_counters_t m_count_total;                 //!< Total count since initialization
        hpmc_counters_t m_count_run_start;             //!< Count saved at run() start
        hpmc_counters_t m_count_step_start;            //!< Count saved at the start of the last step

        //! Find interactions between particles due to overlap and depletion interaction
        /*! \param timestep Current time step
            \param pivot The current pivot point
            \param q The current line reflection axis
            \param line True if this is a line reflection
            \param map Map to lookup new tag from old tag
        */
        virtual void findInteractions(unsigned int timestep, vec3<Scalar> pivot, quat<Scalar> q, bool line,
            const std::map<unsigned int, unsigned int>& map);

        //! Helper function to get interaction range
        virtual Scalar getNominalWidth()
            {
            Scalar nominal_width = m_mc->getMaxCoreDiameter();
            auto patch = m_mc->getPatchInteraction();
            if (patch)
                nominal_width = std::max(nominal_width, patch->getRCut());
            return nominal_width;
            }
    };

template< class Shape >
UpdaterClusters<Shape>::UpdaterClusters(std::shared_ptr<SystemDefinition> sysdef,
                                 std::shared_ptr<IntegratorHPMCMono<Shape> > mc,
                                 unsigned int seed)
        : Updater(sysdef), m_mc(mc), m_seed(seed), m_move_ratio(0.5),
        m_n_particles_old(0)
    {
    m_exec_conf->msg->notice(5) << "Constructing UpdaterClusters" << std::endl;

    // initialize logger and stats
    resetStats();
    }

template< class Shape >
UpdaterClusters<Shape>::~UpdaterClusters()
    {
    m_exec_conf->msg->notice(5) << "Destroying UpdaterClusters" << std::endl;
    }

template< class Shape >
void UpdaterClusters<Shape>::findInteractions(unsigned int timestep, vec3<Scalar> pivot, quat<Scalar> q, bool line,
    const std::map<unsigned int, unsigned int>& map)
    {
    if (m_prof) m_prof->push(m_exec_conf,"Interactions");

    // access parameters
    auto& params = m_mc->getParams();

    // update the image list
    auto image_list = m_mc->updateImageList();
    auto image_hkl = m_mc->getImageHKL();

    // minimum AABB extent
    Scalar min_core_diameter = m_mc->getMinCoreDiameter();

    Index2D overlap_idx = m_mc->getOverlapIndexer();
    ArrayHandle<unsigned int> h_overlaps(m_mc->getInteractionMatrix(), access_location::host, access_mode::read);

    // clear the local bond and rejection lists
    m_overlap.clear();
    m_interact_old_old.clear();
    m_interact_new_old.clear();
    m_interact_new_new.clear();
    m_local_reject.clear();

    auto patch = m_mc->getPatchInteraction();

    Scalar r_cut_patch(0.0);
    if (patch)
        {
        m_energy_old_old.clear();
        m_energy_new_old.clear();
        r_cut_patch = patch->getRCut();
        }

    // cluster according to overlap of excluded volume shells
    // loop over local particles
    unsigned int nptl = m_pdata->getN();

    // access particle data
    ArrayHandle<Scalar4> h_postype(m_pdata->getPositions(), access_location::host, access_mode::read);
    ArrayHandle<Scalar4> h_orientation(m_pdata->getOrientationArray(), access_location::host, access_mode::read);
    ArrayHandle<Scalar> h_diameter(m_pdata->getDiameters(), access_location::host, access_mode::read);
    ArrayHandle<Scalar> h_charge(m_pdata->getCharges(), access_location::host, access_mode::read);
    ArrayHandle<unsigned int> h_tag(m_pdata->getTags(), access_location::host, access_mode::read);
    ArrayHandle<int3> h_image(m_pdata->getImages(), access_location::host, access_mode::read);

    if (patch)
        {
        // test old configuration against itself
        for (unsigned int i = 0; i < m_n_particles_old; ++i)
            {
            unsigned int typ_i = __scalar_as_int(m_postype_backup[i].w);

            vec3<Scalar> pos_i(m_postype_backup[i]);
            quat<Scalar> orientation_i(m_orientation_backup[i]);

            Scalar d_i(m_diameter_backup[i]);
            Scalar charge_i(m_charge_backup[i]);

            // subtract minimum AABB extent from search radius
            OverlapReal R_query = std::max(0.0,r_cut_patch-min_core_diameter/(OverlapReal)2.0);
            detail::AABB aabb_local = detail::AABB(vec3<Scalar>(0,0,0),R_query);

            const unsigned int n_images = image_list.size();

            for (unsigned int cur_image = 0; cur_image < n_images; cur_image++)
                {
                vec3<Scalar> pos_i_image = pos_i + image_list[cur_image];

                detail::AABB aabb_i_image = aabb_local;
                aabb_i_image.translate(pos_i_image);

                // stackless search
                for (unsigned int cur_node_idx = 0; cur_node_idx < m_aabb_tree_old.getNumNodes(); cur_node_idx++)
                    {
                    if (detail::overlap(m_aabb_tree_old.getNodeAABB(cur_node_idx), aabb_i_image))
                        {
                        if (m_aabb_tree_old.isNodeLeaf(cur_node_idx))
                            {
                            for (unsigned int cur_p = 0; cur_p < m_aabb_tree_old.getNodeNumParticles(cur_node_idx); cur_p++)
                                {
                                // read in its position and orientation
                                unsigned int j = m_aabb_tree_old.getNodeParticle(cur_node_idx, cur_p);

                                if (m_tag_backup[i] == m_tag_backup[j] && cur_image == 0) continue;

                                // load the position and orientation of the j particle
                                vec3<Scalar> pos_j = vec3<Scalar>(m_postype_backup[j]);
                                unsigned int typ_j = __scalar_as_int(m_postype_backup[j].w);

                                // put particles in coordinate system of particle i
                                vec3<Scalar> r_ij = pos_j - pos_i_image;
                                Scalar rsq_ij = dot(r_ij, r_ij);

                                if (rsq_ij <= r_cut_patch*r_cut_patch)
                                    {
                                    // the particle pair
                                    auto it = map.find(m_tag_backup[i]);
                                    assert(it != map.end());
                                    unsigned int new_tag_i = it->second;
                                    it = map.find(m_tag_backup[j]);
                                    assert(it!=map.end());
                                    unsigned int new_tag_j = it->second;
                                    auto p = std::make_pair(new_tag_i,new_tag_j);

                                    // if particle interacts in different image already, add to that energy
                                    float U = 0.0;
                                    auto it_energy = m_energy_old_old.find(p);
                                    if (it_energy != m_energy_old_old.end())
                                        U = it_energy->second;

                                    U += patch->energy(r_ij, typ_i,
                                                        quat<float>(orientation_i),
                                                        d_i,
                                                        charge_i,
                                                        typ_j,
                                                        quat<float>(m_orientation_backup[j]),
                                                        m_diameter_backup[j],
                                                        m_charge_backup[j]);

                                    // update map
                                    m_energy_old_old[p] = U;

                                    int3 delta_img = -image_hkl[cur_image] + this->m_image_backup[i] - this->m_image_backup[j];
                                    if ((delta_img.x || delta_img.y || delta_img.z) && line)
                                        {
                                        // if interaction across PBC, reject cluster move
                                        m_local_reject.insert(new_tag_i);
                                        m_local_reject.insert(new_tag_j);
                                        }
                                    } // end if overlap

                                } // end loop over AABB tree leaf
                            } // end is leaf
                        } // end if overlap
                    else
                        {
                        // skip ahead
                        cur_node_idx += m_aabb_tree_old.getNodeSkip(cur_node_idx);
                        }

                    } // end loop over nodes

                } // end loop over images

            } // end loop over old configuration
        }

    // loop over new configuration
    for (unsigned int i = 0; i < nptl; ++i)
        {
        unsigned int typ_i = __scalar_as_int(h_postype.data[i].w);

        vec3<Scalar> pos_i_new(h_postype.data[i]);
        quat<Scalar> orientation_i_new(h_orientation.data[i]);

        Shape shape_i(orientation_i_new, params[typ_i]);
        Scalar r_excl_i = shape_i.getCircumsphereDiameter()/Scalar(2.0);

        // check for overlap at mirrored position, with other particles in old configuration
        detail::AABB aabb_i = shape_i.getAABB(pos_i_new);

        // All image boxes (including the primary)
        const unsigned int n_images = image_list.size();

        // check against old
        for (unsigned int cur_image = 0; cur_image < n_images; cur_image++)
            {
            vec3<Scalar> pos_i_image = pos_i_new + image_list[cur_image];

            detail::AABB aabb_i_image = aabb_i;
            aabb_i_image.translate(image_list[cur_image]);

            // stackless search
            for (unsigned int cur_node_idx = 0; cur_node_idx < m_aabb_tree_old.getNumNodes(); cur_node_idx++)
                {
                if (detail::overlap(m_aabb_tree_old.getNodeAABB(cur_node_idx), aabb_i_image))
                    {
                    if (m_aabb_tree_old.isNodeLeaf(cur_node_idx))
                        {
                        for (unsigned int cur_p = 0; cur_p < m_aabb_tree_old.getNodeNumParticles(cur_node_idx); cur_p++)
                            {
                            // read in its position and orientation
                            unsigned int j = m_aabb_tree_old.getNodeParticle(cur_node_idx, cur_p);

                            auto it = map.find(m_tag_backup[j]);
                            assert(it != map.end());
                            unsigned int new_tag_j = it->second;

                            if (h_tag.data[i] == new_tag_j && cur_image == 0) continue;

                            // load the position and orientation of the j particle
                            vec3<Scalar> pos_j = vec3<Scalar>(m_postype_backup[j]);
                            unsigned int typ_j = __scalar_as_int(m_postype_backup[j].w);
                            Shape shape_j(quat<Scalar>(m_orientation_backup[j]), params[typ_j]);

                            // put particles in coordinate system of particle i
                            vec3<Scalar> r_ij = pos_j - pos_i_image;

                            // check for circumsphere overlap
                            Scalar r_excl_j = shape_j.getCircumsphereDiameter()/Scalar(2.0);
                            Scalar RaRb = r_excl_i + r_excl_j;
                            Scalar rsq_ij = dot(r_ij, r_ij);

                            unsigned int err = 0;
                            if (rsq_ij <= RaRb*RaRb)
                                {
                                if (h_overlaps.data[overlap_idx(typ_i,typ_j)]
                                    && test_overlap(r_ij, shape_i, shape_j, err))
                                    {
                                    // add connection
                                    m_overlap.insert(std::make_pair(h_tag.data[i],new_tag_j));

                                    int3 delta_img = -image_hkl[cur_image] + h_image.data[i] - this->m_image_backup[j];
                                    if ((delta_img.x || delta_img.y || delta_img.z) && line)
                                        {
                                        // if interaction across PBC, reject cluster move
                                        m_local_reject.insert(h_tag.data[i]);
                                        m_local_reject.insert(new_tag_j);
                                        }
                                    } // end if overlap
                                }

                            } // end loop over AABB tree leaf
                        } // end is leaf
                    } // end if overlap
                else
                    {
                    // skip ahead
                    cur_node_idx += m_aabb_tree_old.getNodeSkip(cur_node_idx);
                    }

                } // end loop over nodes
            } // end loop over images

        if (patch)
            {
            // subtract minimum AABB extent from search radius
            OverlapReal R_query = std::max(0.0,r_cut_patch-min_core_diameter/(OverlapReal)2.0);
            detail::AABB aabb_local = detail::AABB(vec3<Scalar>(0,0,0),R_query);

            // compute V(r'-r)
            for (unsigned int cur_image = 0; cur_image < n_images; cur_image++)
                {
                vec3<Scalar> pos_i_image = pos_i_new + image_list[cur_image];

                detail::AABB aabb_i_image = aabb_local;
                aabb_i_image.translate(pos_i_image);

                // stackless search
                for (unsigned int cur_node_idx = 0; cur_node_idx < m_aabb_tree_old.getNumNodes(); cur_node_idx++)
                    {
                    if (detail::overlap(m_aabb_tree_old.getNodeAABB(cur_node_idx), aabb_i_image))
                        {
                        if (m_aabb_tree_old.isNodeLeaf(cur_node_idx))
                            {
                            for (unsigned int cur_p = 0; cur_p < m_aabb_tree_old.getNodeNumParticles(cur_node_idx); cur_p++)
                                {
                                // read in its position and orientation
                                unsigned int j = m_aabb_tree_old.getNodeParticle(cur_node_idx, cur_p);

                                auto it = map.find(m_tag_backup[j]);
                                assert(it != map.end());
                                unsigned int new_tag_j = it->second;

                                if (h_tag.data[i] == new_tag_j && cur_image == 0) continue;

                                vec3<Scalar> pos_j(m_postype_backup[j]);
                                unsigned int typ_j = __scalar_as_int(m_postype_backup[j].w);

                                // put particles in coordinate system of particle i
                                vec3<Scalar> r_ij = pos_j - pos_i_image;

                                // check for excluded volume sphere overlap
                                Scalar rsq_ij = dot(r_ij, r_ij);

                                if (rsq_ij <= r_cut_patch*r_cut_patch)
                                    {
                                    auto p = std::make_pair(h_tag.data[i], new_tag_j);

                                    // if particle interacts in different image already, add to that energy
                                    float U = 0.0;
                                    auto it_energy = m_energy_new_old.find(p);
                                    if (it_energy != m_energy_new_old.end())
                                        U = it_energy->second;

                                    U += patch->energy(r_ij, typ_i,
                                                            quat<float>(shape_i.orientation),
                                                            h_diameter.data[i],
                                                            h_charge.data[i],
                                                            typ_j,
                                                            quat<float>(m_orientation_backup[j]),
                                                            m_diameter_backup[j],
                                                            m_charge_backup[j]);

                                    // update map
                                    m_energy_new_old[p] = U;

                                    int3 delta_img = -image_hkl[cur_image] + h_image.data[i] - this->m_image_backup[j];
                                    if ((delta_img.x || delta_img.y || delta_img.z) && line)
                                    if (cur_image && line)
                                        {
                                        // if interaction across PBC, reject cluster move
                                        m_local_reject.insert(h_tag.data[i]);
                                        m_local_reject.insert(new_tag_j);
                                        }
                                    }
                                } // end loop over AABB tree leaf
                            } // end is leaf
                        } // end if overlap
                    else
                        {
                        // skip ahead
                        cur_node_idx += m_aabb_tree_old.getNodeSkip(cur_node_idx);
                        }

                    } // end loop over nodes

                } // end loop over images
            } // end if patch
        } // end loop over local particles

    if (line)
        {
        // locality data in new configuration
        const detail::AABBTree& aabb_tree = m_mc->buildAABBTree();

        // check if particles are interacting in the new configuration
        for (unsigned int i = 0; i < nptl; ++i)
            {
            unsigned int typ_i = __scalar_as_int(h_postype.data[i].w);

            vec3<Scalar> pos_i_new(h_postype.data[i]);
            quat<Scalar> orientation_i_new(h_orientation.data[i]);

            Shape shape_i(orientation_i_new, params[typ_i]);
            Scalar r_excl_i = shape_i.getCircumsphereDiameter()/Scalar(2.0);

            // subtract minimum AABB extent from search radius
            OverlapReal R_query = std::max(r_excl_i,r_cut_patch-min_core_diameter/(OverlapReal)2.0);
            detail::AABB aabb_i = detail::AABB(pos_i_new,R_query);

            // All image boxes (including the primary)
            const unsigned int n_images = image_list.size();

            // check against new AABB tree
            for (unsigned int cur_image = 0; cur_image < n_images; cur_image++)
                {
                vec3<Scalar> pos_i_image = pos_i_new + image_list[cur_image];

                detail::AABB aabb_i_image = aabb_i;
                aabb_i_image.translate(image_list[cur_image]);

                // stackless search
                for (unsigned int cur_node_idx = 0; cur_node_idx < aabb_tree.getNumNodes(); cur_node_idx++)
                    {
                    if (detail::overlap(aabb_tree.getNodeAABB(cur_node_idx), aabb_i_image))
                        {
                        if (aabb_tree.isNodeLeaf(cur_node_idx))
                            {
                            for (unsigned int cur_p = 0; cur_p < aabb_tree.getNodeNumParticles(cur_node_idx); cur_p++)
                                {
                                // read in its position and orientation
                                unsigned int j = aabb_tree.getNodeParticle(cur_node_idx, cur_p);

                                // no trivial bonds
                                if (h_tag.data[i] == h_tag.data[j]) continue;

                                // load the position and orientation of the j particle
                                vec3<Scalar> pos_j = vec3<Scalar>(h_postype.data[j]);
                                unsigned int typ_j = __scalar_as_int(h_postype.data[j].w);
                                Shape shape_j(quat<Scalar>(h_orientation.data[j]), params[typ_j]);

                                // put particles in coordinate system of particle i
                                vec3<Scalar> r_ij = pos_j - pos_i_image;

                                // check for circumsphere overlap
                                Scalar r_excl_j = shape_j.getCircumsphereDiameter()/Scalar(2.0);
                                Scalar RaRb = r_excl_i + r_excl_j;
                                Scalar rsq_ij = dot(r_ij, r_ij);

                                bool interact_patch = patch && rsq_ij <= r_cut_patch*r_cut_patch;

                                unsigned int err = 0;
                                if (interact_patch || (rsq_ij <= RaRb*RaRb && h_overlaps.data[overlap_idx(typ_i,typ_j)]
                                        && test_overlap(r_ij, shape_i, shape_j, err)))
                                    {
                                    int3 delta_img = -image_hkl[cur_image] + h_image.data[i] - h_image.data[j];
                                    if ((delta_img.x || delta_img.y || delta_img.z) && line)
                                        {
                                        // add to list
                                        m_interact_new_new.insert(std::make_pair(h_tag.data[i],h_tag.data[j]));
                                        }
                                    } // end if overlap

                                } // end loop over AABB tree leaf
                            } // end is leaf
                        } // end if overlap
                    else
                        {
                        // skip ahead
                        cur_node_idx += aabb_tree.getNodeSkip(cur_node_idx);
                        }

                    } // end loop over nodes
                } // end loop over images
            } // end loop over local particles
        } // end if line transformation
    if (m_prof) m_prof->pop(m_exec_conf);
    }

/*! Perform a cluster move
    \param timestep Current time step of the simulation
*/
template< class Shape >
void UpdaterClusters<Shape>::update(unsigned int timestep)
    {
    m_exec_conf->msg->notice(10) << timestep << " UpdaterClusters" << std::endl;

    m_count_step_start = m_count_total;

    // if no particles, exit early
    if (! m_pdata->getNGlobal()) return;

    if (m_prof) m_prof->push(m_exec_conf,"HPMC Clusters");

    // save a copy of the old configuration
    m_n_particles_old = m_pdata->getN();

    unsigned int nptl = m_pdata->getN()+m_pdata->getNGhosts();
    m_postype_backup.resize(nptl);
    m_orientation_backup.resize(nptl);
    m_diameter_backup.resize(nptl);
    m_charge_backup.resize(nptl);
    m_tag_backup.resize(nptl);
    m_image_backup.resize(nptl);

        {
        ArrayHandle<Scalar4> h_postype(m_pdata->getPositions(), access_location::host, access_mode::read);
        ArrayHandle<Scalar4> h_orientation(m_pdata->getOrientationArray(), access_location::host, access_mode::read);
        ArrayHandle<Scalar> h_diameter(m_pdata->getDiameters(), access_location::host, access_mode::read);
        ArrayHandle<Scalar> h_charge(m_pdata->getDiameters(), access_location::host, access_mode::read);
        ArrayHandle<unsigned int> h_tag(m_pdata->getTags(), access_location::host, access_mode::read);
        ArrayHandle<int3> h_image(m_pdata->getImages(), access_location::host, access_mode::read);

        for (unsigned int i = 0; i < nptl; ++i)
            {
            m_postype_backup[i] = h_postype.data[i];
            m_orientation_backup[i] = h_orientation.data[i];
            m_diameter_backup[i] = h_diameter.data[i];
            m_charge_backup[i] = h_charge.data[i];
            m_tag_backup[i] = h_tag.data[i];
            // reset image
            m_image_backup[i] = make_int3(0,0,0);
            }
        }

    if (m_prof) m_prof->push(m_exec_conf,"Transform");

    // generate the move, select a pivot
    hoomd::detail::Saru rng(timestep, this->m_seed, 0x09365bf5);
    BoxDim box = m_pdata->getGlobalBox();
    vec3<Scalar> pivot(0,0,0);

    // is this a line reflection?
    bool line = m_mc->hasOrientation() || (rng.template s<Scalar>() > m_move_ratio);

    quat<Scalar> q;

    Scalar3 f;
    f.x = rng.template s<Scalar>();
    f.y = rng.template s<Scalar>();
    if (m_sysdef->getNDimensions() == 3)
        {
        f.z = rng.template s<Scalar>();
        }
    else
        {
        f.z = 0.5;
        }

    pivot = vec3<Scalar>(box.makeCoordinates(f));
    if (m_sysdef->getNDimensions() == 2)
        {
        // force z component to be zero
        pivot.z = 0.0;
        }

    if (line)
        {
        // random normalized vector
        vec3<Scalar> n;

        if (m_sysdef->getNDimensions() == 3)
            {
            Scalar theta = rng.template s<Scalar>(Scalar(0.0),Scalar(2.0*M_PI));
            Scalar z = rng.template s<Scalar>(Scalar(-1.0),Scalar(1.0));
            n = vec3<Scalar>(fast::sqrt(Scalar(1.0)-z*z)*fast::cos(theta),fast::sqrt(Scalar(1.0)-z*z)*fast::sin(theta),z);
            }
        else
            {
            // reflection around z axis
            n = vec3<Scalar>(0,0,1);
            }

        // line reflection
        q = quat<Scalar>(0,n);
        }

    SnapshotParticleData<Scalar> snap(m_pdata->getNGlobal());

    // obtain particle data from all ranks

    // save origin information
    Scalar3 origin = m_pdata->getOrigin();
    int3 origin_image = m_pdata->getOriginImage();

    // take a snapshot, and save tag->snap idx mapping
    auto map = m_pdata->takeSnapshot(snap);

    #ifdef ENABLE_MPI
    if (m_comm)
        {
        // we need the particle tag -> snapshot idx map on all ranks
        bcast(map, 0, m_exec_conf->getMPICommunicator());
        }
    #endif

    // keep a backup copy
    SnapshotParticleData<Scalar> snap_old = snap;

    // precalculate the grid shift
    Scalar nominal_width = this->getNominalWidth();

    // transform all particles on rank zero
    bool master = !m_exec_conf->getRank();

    // compute the width of the active region
    Scalar3 npd = box.getNearestPlaneDistance();
    Scalar3 range = nominal_width / npd;

    if (m_sysdef->getNDimensions() == 2)
        {
        // no interaction along z
        range.z = 0;
        }

    // reset list of rejected particles
    m_ptl_reject.clear();

    if (master)
        {
        // access parameters
        auto& params = m_mc->getParams();

        // create a copy of the box without periodic boundaries
        BoxDim global_box_nonperiodic = box;
        global_box_nonperiodic.setPeriodic(m_pdata->getBox().getPeriodic());

        for (unsigned int i = 0; i < snap.size; ++i)
            {
            // reset image
            snap.image[i] = make_int3(0,0,0);

            // if the particle falls outside the active volume of global_box_nonperiodic, reject
            if (line && !isActive(vec_to_scalar3(snap.pos[i]), global_box_nonperiodic, range))
                {
                m_ptl_reject.insert(i);
                }

            if (!line)
                {
                // point reflection
                snap.pos[i] = pivot-(snap.pos[i]-pivot);
                }
            else
                {
                // line reflection
                snap.pos[i] = lineReflection(snap.pos[i], pivot, q);
                Shape shape_i(snap.orientation[i], params[snap.type[i]]);
                if (shape_i.hasOrientation())
                    snap.orientation[i] = q*snap.orientation[i];
                }

            // reject if outside active volume of box at new position
            if (line && !isActive(vec_to_scalar3(snap.pos[i]), global_box_nonperiodic, range))
                {
                m_ptl_reject.insert(i);
                }

            // wrap particle back into box
            box.wrap(snap.pos[i], snap.image[i]);
            }
        }

    if (m_prof) m_prof->pop(m_exec_conf);

    // store old locality data
    m_aabb_tree_old = m_mc->buildAABBTree();

    // reload particle data
    // now all tags will be consecutive
    m_pdata->initializeFromSnapshot(snap);

    if (m_prof) m_prof->pop(m_exec_conf);

    // update ghosts & signal that AABB tree is invalid
    m_mc->communicate(true);

    if (m_prof) m_prof->push(m_exec_conf,"HPMC Clusters");

    // determine which particles interact
    findInteractions(timestep, pivot, q, line, map);

    if (m_prof) m_prof->push(m_exec_conf,"Move");

    // collect interactions on rank 0
    std::vector< std::set<std::pair<unsigned int, unsigned int> > > all_overlap;
    std::vector< std::set<std::pair<unsigned int, unsigned int> > > all_interact_old_old;
    std::vector< std::set<std::pair<unsigned int, unsigned int> > > all_interact_new_old;
    std::vector< std::set<std::pair<unsigned int, unsigned int> > > all_interact_new_new;
    std::vector< std::set<unsigned int> > all_local_reject;

    std::vector< std::map<std::pair<unsigned int, unsigned int>, float> > all_energy_old_old;
    std::vector< std::map<std::pair<unsigned int, unsigned int>, float> > all_energy_new_old;

    #ifdef ENABLE_MPI
    if (m_comm)
        {
        gather_v(m_overlap, all_overlap, 0, m_exec_conf->getMPICommunicator());
        gather_v(m_interact_old_old, all_interact_old_old, 0, m_exec_conf->getMPICommunicator());
        gather_v(m_interact_new_old, all_interact_new_old, 0, m_exec_conf->getMPICommunicator());
        gather_v(m_interact_new_new, all_interact_new_new, 0, m_exec_conf->getMPICommunicator());
        gather_v(m_local_reject, all_local_reject, 0, m_exec_conf->getMPICommunicator());
        }
    else
    #endif
        {
        all_overlap.push_back(m_overlap);
        all_interact_old_old.push_back(m_interact_old_old);
        all_interact_new_old.push_back(m_interact_new_old);
        all_interact_new_new.push_back(m_interact_new_new);
        all_local_reject.push_back(m_local_reject);
        }

    if (m_mc->getPatchInteraction())
        {
        // collect energies on rank 0
        #ifdef ENABLE_MPI
        if (m_comm)
            {
            gather_v(m_energy_old_old, all_energy_old_old, 0, m_exec_conf->getMPICommunicator());
            gather_v(m_energy_new_old, all_energy_new_old, 0, m_exec_conf->getMPICommunicator());
            }
        else
        #endif
            {
            all_energy_old_old.push_back(m_energy_old_old);
            all_energy_new_old.push_back(m_energy_new_old);
            }
        }

    if (master)
        {
        // fill in the cluster bonds, using bond formation probability defined in Liu and Luijten
        m_G = detail::Graph(snap.size);

        // complete the list of rejected particles
        for (auto it_i = all_local_reject.begin(); it_i != all_local_reject.end(); ++it_i)
            {
            for (auto it_j = it_i->begin(); it_j != it_i->end(); ++it_j)
                {
                m_ptl_reject.insert(*it_j);
                }
            }

        for (auto it_i = all_overlap.begin(); it_i != all_overlap.end(); ++it_i)
            {
            for (auto it_j = it_i->begin(); it_j != it_i->end(); ++it_j)
                {
                // particles in the new configuration overlapping with the old one are transformed as part of the same cluster
                m_G.addEdge(it_j->first, it_j->second);
                }
            }

        // interactions due to hard depletant-excluded volume overlaps (not used in base class)
        for (auto it_i = all_interact_old_old.begin(); it_i != all_interact_old_old.end(); ++it_i)
            {
            for (auto it_j = it_i->begin(); it_j != it_i->end(); ++it_j)
                {
                unsigned int i = it_j->first;
                unsigned int j = it_j->second;

                // do they interact when both particles are in the new configuration?
                bool interact_new_new = false;
                for (auto it_k = all_interact_new_new.begin(); it_k != all_interact_new_new.end(); ++it_k)
                    {
                    if (it_k->find(std::make_pair(i,j)) != it_k->end() ||
                        it_k->find(std::make_pair(j,i)) != it_k->end())
                        {
                        interact_new_new = true;
                        break;
                        }
                    }

                if (interact_new_new)
                    {
                    m_ptl_reject.insert(i);
                    m_ptl_reject.insert(j);
                    }

                m_G.addEdge(i, j);
                }
            }

         for (auto it_i = all_interact_new_old.begin(); it_i != all_interact_new_old.end(); ++it_i)
            {
            for (auto it_j = it_i->begin(); it_j != it_i->end(); ++it_j)
                {
                unsigned int i = it_j->first;
                unsigned int j = it_j->second;

                // do they interact when both particles are in the new configuration?
                bool interact_new_new = false;
                for (auto it_k = all_interact_new_new.begin(); it_k != all_interact_new_new.end(); ++it_k)
                    {
                    if (it_k->find(std::make_pair(i,j)) != it_k->end() ||
                        it_k->find(std::make_pair(j,i)) != it_k->end())
                        {
                        interact_new_new = true;
                        break;
                        }
                    }

                if (interact_new_new)
                    {
                    m_ptl_reject.insert(i);
                    m_ptl_reject.insert(j);
                    }

                m_G.addEdge(i, j);
                }
            }

        if (m_mc->getPatchInteraction())
            {
            // sum up interaction energies
            std::map< std::pair<unsigned int, unsigned int>, float> delta_U;

            for (auto it_i = all_energy_old_old.begin(); it_i != all_energy_old_old.end(); ++it_i)
                {
                for (auto it_j = it_i->begin(); it_j != it_i->end(); ++it_j)
                    {
                    float delU = -it_j->second;
                    unsigned int i = it_j->first.first;
                    unsigned int j = it_j->first.second;

                    auto p = std::make_pair(i,j);

                    // add to energy
                    auto it = delta_U.find(p);
                    if (it != delta_U.end())
                        delU += it->second;

                    // update map with new interaction energy
                    delta_U[p] = delU;
                    }
                }

            for (auto it_i = all_energy_new_old.begin(); it_i != all_energy_new_old.end(); ++it_i)
                {
                for (auto it_j = it_i->begin(); it_j != it_i->end(); ++it_j)
                    {
                    float delU = it_j->second;
                    unsigned int i = it_j->first.first;
                    unsigned int j = it_j->first.second;

                    // consider each pair uniquely
                    auto p = std::make_pair(i,j);

                    // add to energy
                    auto it = delta_U.find(p);
                    if (it != delta_U.end())
                        delU += it->second;

                    // update map with new interaction energy
                    delta_U[p] = delU;
                    }
                }

            for (auto it = delta_U.begin(); it != delta_U.end(); ++it)
                {
                float delU = it->second;
                unsigned int i = it->first.first;
                unsigned int j = it->first.second;

                float pij = 1.0f-exp(-delU);
                if (rng.f() <= pij) // GCA
                    {
                    // add bond
                    m_G.addEdge(i,j);

                    // do they interact when both particles are in the new configuration?
                    bool interact_new_new = false;
                    for (auto it_k = all_interact_new_new.begin(); it_k != all_interact_new_new.end(); ++it_k)
                        {
                        if (it_k->find(std::make_pair(i,j)) != it_k->end() ||
                            it_k->find(std::make_pair(j,i)) != it_k->end())
                            {
                            interact_new_new = true;
                            break;
                            }
                        }
                    if (interact_new_new)
                        {
                        m_ptl_reject.insert(i);
                        m_ptl_reject.insert(j);
                        }
                    }
                }
            } // end if (patch)

        // compute connected components
        m_clusters.clear();
        m_G.connectedComponents(m_clusters);

        // move every cluster independently
        for (unsigned int icluster = 0; icluster < m_clusters.size(); icluster++)
            {
            // if any particle in the cluster is rejected, the cluster is not transformed
            bool reject = false;
            for (auto it = m_clusters[icluster].begin(); it != m_clusters[icluster].end(); ++it)
                {
                if (m_ptl_reject.find(*it) != m_ptl_reject.end())
                    reject = true;
                }

            if (reject)
                {
                // revert cluster
                for (auto it = m_clusters[icluster].begin(); it != m_clusters[icluster].end(); ++it)
                    {
                    // particle index
                    unsigned int i = *it;

                    snap.pos[i] = snap_old.pos[i];
                    snap.orientation[i] = snap_old.orientation[i];
                    }

                // use translate for pivot moves, rotate for line reflections
                if (line)
                    m_count_total.rotate_reject_count++;
                else
                    m_count_total.translate_reject_count++;
                }
            else
                {
                if (line)
                    m_count_total.rotate_accept_count++;
                else
                    m_count_total.translate_accept_count++;
                }
            } // end loop over clusters

        for (unsigned int i = 0; i < snap.size; i++)
            {
            // wrap back into box
            box.wrap(snap.pos[i],snap.image[i]);

            // restore image
            snap.image[i] += snap_old.image[i];
            }

        } // if master

    // finally re-initialize particle data
    m_pdata->initializeFromSnapshot(snap);

    // restore origin, after initializing from translated positions
    m_pdata->setOrigin(origin,origin_image);

    if (m_prof) m_prof->pop(m_exec_conf);

    m_mc->communicate(true);
    }


template < class Shape> void export_UpdaterClusters(pybind11::module& m, const std::string& name)
    {
    pybind11::class_< UpdaterClusters<Shape>, std::shared_ptr< UpdaterClusters<Shape> > >(m, name.c_str(), pybind11::base<Updater>())
          .def( pybind11::init< std::shared_ptr<SystemDefinition>,
                         std::shared_ptr< IntegratorHPMCMono<Shape> >,
                         unsigned int >())
        .def("getCounters", &UpdaterClusters<Shape>::getCounters)
        .def("setMoveRatio", &UpdaterClusters<Shape>::setMoveRatio)
    ;
    }

} // end namespace hpmc

#endif // _UPDATER_HPMC_CLUSTERS_
