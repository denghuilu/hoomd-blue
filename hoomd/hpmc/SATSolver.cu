#include "SATSolver.cuh"

#include <hip/hip_runtime.h>

#include <thrust/device_ptr.h>
#include <thrust/sort.h>
#include <thrust/copy.h>
#include <thrust/binary_search.h>
#include <thrust/adjacent_difference.h>
#include <thrust/iterator/counting_iterator.h>
#include <thrust/iterator/constant_iterator.h>
#include <thrust/iterator/discard_iterator.h>

#include <cub/cub.cuh>
#include <cub/iterator/discard_output_iterator.cuh>

#include "IntegratorHPMCMonoGPUTypes.cuh"

#include "hoomd/extern/ECL.cuh"

namespace hpmc {

namespace gpu {

namespace kernel {

__device__ inline bool update_watchlist(
    const unsigned int false_literal,
    unsigned int *d_watch,
    unsigned int *d_next_clause,
    const unsigned int *d_literals,
    const unsigned int *d_assignment,
    unsigned int *d_next,
    unsigned int &h,
    unsigned int &t)
    {
    unsigned int c = d_watch[false_literal];

    // false_literal is no longer being watched
    d_watch[false_literal] = SAT_sentinel;

    // update the clauses watching it to a different watched literal
    while (c != SAT_sentinel)
        {
        unsigned int next = d_next_clause[c];

        bool found_alternative = false;
        unsigned int j = c;
        while (true)
            {
            unsigned int alternative = d_literals[j++];
            if (alternative == SAT_sentinel)
                break; // end of clause

            unsigned int v = alternative >> 1;
            unsigned int a = alternative & 1;
            if (d_assignment[v] == SAT_sentinel || d_assignment[v] == a ^ 1)
                {
                found_alternative = true;

                // the variable corresponding to 'alternative' might become active at this point,
                // because it might not be watched anywhere else. In such a case, we insert it at the
                // 'beginning' of the active ring (that is, just after t)
                if (d_assignment[v] == SAT_sentinel && d_watch[v << 1] == SAT_sentinel && d_watch[(v << 1) | 1] == SAT_sentinel)
                    {
                    if (t == SAT_sentinel)
                        {
                        t = h = v;
                        d_next[t] = h;
                        }
                    else
                        {
                        d_next[v] = h;
                        h = v;
                        d_next[t] = h;
                        }
                    }

                // insert clause at begining of alternative literal's watch list
                d_next_clause[c] = d_watch[alternative];
                d_watch[alternative] = c;
                break;
                }
            }

        if (!found_alternative)
            return false; // should never get here

        c = next;
        }

    return true;
    }

// Returns true if literal is being watched by a unit clause
__device__ inline bool is_unit(
    const unsigned int literal,
    const unsigned int *d_watch,
    const unsigned int *d_next_clause,
    const unsigned int *d_literals,
    const unsigned int *d_assignment)
    {
    unsigned int c = d_watch[literal];

    while (c != SAT_sentinel)
        {
        bool unit_clause = true;
        unsigned int j = c;
        while (true)
            {
            unsigned int l = d_literals[j++];

            if (l == SAT_sentinel)
                break; // end of clause

            if (l == literal)
                continue;

            unsigned int v = l >> 1;
            unsigned int a = l & 1;

            // if there is a different literal that is either unassigned or true, this clause can not be a unit clause
            if (d_assignment[v] == SAT_sentinel || d_assignment[v] == a ^ 1)
                {
                unit_clause = false;
                break;
                }
            }

        if (unit_clause)
            return true;

        c = d_next_clause[c];
        }

    return false;
    }

__global__ void solve_sat(
    unsigned int *d_watch,
    unsigned int *d_next_clause,
    unsigned int *d_next,
    unsigned int *d_h,
    const unsigned int *d_head,
    const unsigned int *d_literals,
    unsigned int *d_assignment,
    unsigned int *d_state,
    const unsigned int *d_representative,
    const unsigned int n_variables,
    unsigned int *d_unsat,
    unsigned int *d_heap)
    {
    unsigned int node_idx = threadIdx.x + blockIdx.x*blockDim.x;

    if (node_idx >= n_variables)
        return;

    // start from the representatives of every component, all other threads just exit
    if (d_representative[node_idx] != node_idx)
        {
        return;
        }

    unsigned int h = d_head[node_idx];

    // chase pointers until we find a tail for the ring buffer
    unsigned int v = h;
    unsigned int n = 0;
    unsigned int t = SAT_sentinel;
    while (v != SAT_sentinel)
        {
        t = v;
        v = d_next[v];
        n++; // the size of the component
        }
    if (t != SAT_sentinel)
        d_next[t] = h;

    // allocate scratch memory for this component
    unsigned int component_start = atomicAdd(d_heap, n);
    unsigned int d = component_start;

    while (true)
        {
        if (t == SAT_sentinel)
            return; // SAT

        // fetch next variable
        unsigned int k = t;

        bool backtrack = false;
        bool unit = false;
        do
            {
            // look for unit clauses
            h = d_next[k];

            bool is_h_unit = is_unit(h << 1,
                                     d_watch,
                                     d_next_clause,
                                     d_literals,
                                     d_assignment);
            bool is_neg_h_unit = is_unit((h << 1) | 1,
                                     d_watch,
                                     d_next_clause,
                                     d_literals,
                                     d_assignment);

            unsigned int f = is_h_unit + (is_neg_h_unit << 1);

            if (f == 1 || f == 2)
                {
                // one of the two literals is true
                d_state[d] = f + 3;
                t = k;
                unit = true;
                break;
                }
            else if (f == 3)
                {
                // conflict
                backtrack = true;
                break;
                }

            k = h;
            }
        while (h != t);

        if (!backtrack && !unit)
            {
            // two way branch
            h = d_next[t];
            d_state[d] = (d_watch[h << 1] == SAT_sentinel) ||
                         (d_watch[(h << 1) | 1] != SAT_sentinel);
            }

        if (!backtrack)
            {
            // move on
            d_h[d++] = k = h;

            if (t == k)
                {
                t = SAT_sentinel;
                }
            else
                {
                // delete k from ring
                d_next[t] = h = d_next[k];
                }
            }
        else
            {
            t = k;

            while (d > component_start && d_state[d-1] >= 2)
                {
                k = d_h[d-1];
                d_assignment[k] = SAT_sentinel;
                if (d_watch[k << 1] != SAT_sentinel || d_watch[(k << 1) | 1] != SAT_sentinel)
                    {
                    d_next[k] = h;
                    h = k;
                    d_next[t] = h;
                    }

                d--;
                }

            if (d == component_start)
                {
                // can't backtrack further, no solutions
                atomicAdd(d_unsat, 1);
                return;
                }
            else
                {
                // backtrack
                d_state[d-1] = 3 - d_state[d-1];
                k = d_h[d-1];
                }
            }

        // update watches
        unsigned int b = (d_state[d-1] + 1) & 1;
        d_assignment[k] = b;
        update_watchlist((k << 1) | b,
                         d_watch,
                         d_next_clause,
                         d_literals,
                         d_assignment,
                         d_next,
                         h,
                         t);
        }
    }

__global__ void setup_watch_list(
    unsigned int n_variables,
    const unsigned int maxn_literals,
    const unsigned int *d_literals,
    const unsigned int *d_n_literals,
    unsigned int *d_watch,
    unsigned int *d_next_clause)
    {
    unsigned int tidx = threadIdx.x + blockDim.x*blockIdx.x;

    if (tidx >= n_variables)
        return;

    // go through the literals associated with this variable and pick the first literal of every clause
    unsigned nlit = d_n_literals[tidx];

    bool first = true;
    for (unsigned int i = 0; i < nlit; ++i)
        {
        unsigned int l = d_literals[tidx*maxn_literals+i];

        if (l == SAT_sentinel)
            {
            first = true;
            continue;
            }

        if (first)
            {
            // append the clause to the singly linked list for this literal
            unsigned int c = tidx*maxn_literals+i;
            unsigned int p = atomicCAS(&d_watch[l], SAT_sentinel, c);
            while (p != SAT_sentinel)
                {
                p = atomicCAS(&d_next_clause[p], SAT_sentinel, c);
                }
            }

        first = false;
        }
    }

// Initialize the active list for every component.
__global__ void initialize_components(
    unsigned int *d_watch,
    unsigned int *d_assignment,
    const unsigned int *d_component_ptr,
    const unsigned int n_variables,
    unsigned int *d_representative,
    unsigned int *d_head,
    unsigned int *d_next)
    {
    unsigned int node_idx = threadIdx.x + blockIdx.x*blockDim.x;

    if (node_idx >= n_variables)
        return;

    // jump to the node with the lowest index in this component, which is its label
    unsigned int next, vstat = d_component_ptr[node_idx];
    while (vstat > (next = d_component_ptr[vstat]))
        {
        vstat = next;
        }
    unsigned int component = vstat;

    // store the reprentative for this node's component in global mem
    d_representative[node_idx] = component;

    // assign a sentinel value to the variable for this node
    d_assignment[node_idx] = SAT_sentinel;

    if (d_watch[node_idx << 1] != SAT_sentinel || d_watch[(node_idx << 1) | 1] != SAT_sentinel)
        {
        // append ourselves to the linked list
        unsigned int p = atomicCAS(&d_head[component], SAT_sentinel, node_idx);
        while (p != SAT_sentinel)
            {
            p = atomicCAS(&d_next[p], SAT_sentinel, node_idx);
            }
        }
    }

__global__ void find_dependencies(
    const unsigned int n_variables,
    const unsigned int *d_n_literals,
    const unsigned int *d_literals,
    const unsigned int maxn_literals,
    unsigned int *d_n_elem,
    unsigned int *d_rowidx,
    unsigned int *d_colidx,
    const unsigned int max_n_elem)
    {
    const unsigned int tidx = threadIdx.x + blockIdx.x*blockDim.x;

    if (tidx >= n_variables)
        return;

    unsigned int nlit = d_n_literals[tidx];
    unsigned int prev = SAT_sentinel;

    // merge all elements in each clause asssociated with this variable, generating 2(n-1) edges per clause
    for (unsigned int j = 0; j < nlit; ++j)
        {
        unsigned int l = d_literals[tidx*maxn_literals+j];

        if (prev != SAT_sentinel && l != SAT_sentinel)
            {
            unsigned int v = l >> 1;
            unsigned int w = prev >> 1;

            // add bidirectional edge
            unsigned int k = atomicAdd(d_n_elem, 2);
            if (k + 1 < max_n_elem)
                {
                d_rowidx[k] = d_colidx[k+1] = v;
                d_colidx[k] = d_rowidx[k+1] = w;
                }
            }

        prev = l;
        }
    }

} //end namespace kernel

void identify_connected_components(
    const unsigned int maxn_literals,
    const unsigned int *d_literals,
    const unsigned int *d_n_literals,
    unsigned int *d_n_elem,
    unsigned int &n_elem,
    const unsigned int max_n_elem,
    unsigned int *d_rowidx,
    unsigned int *d_rowidx_alt,
    unsigned int *d_colidx,
    unsigned int *d_colidx_alt,
    unsigned int *d_csr_row_ptr,
    const unsigned int n_variables,
    unsigned int *d_component_ptr,
    unsigned int *d_work,
    const hipDeviceProp_t devprop,
    const unsigned int block_size,
    CachedAllocator &alloc)
    {
    hipMemsetAsync(d_n_elem, 0, sizeof(unsigned int));

    // fill the connnectivity matrix
    hipLaunchKernelGGL(kernel::find_dependencies, n_variables/block_size + 1, block_size, 0, 0,
        n_variables,
        d_n_literals,
        d_literals,
        maxn_literals,
        d_n_elem,
        d_rowidx,
        d_colidx,
        max_n_elem);

    // construct a CSR matrix
    hipMemcpy(&n_elem, d_n_elem, sizeof(unsigned int), hipMemcpyDeviceToHost);

    if (n_elem > max_n_elem)
        return;

    // COO -> CSR
    cub::DoubleBuffer<unsigned int> d_keys(d_rowidx, d_rowidx_alt);
    cub::DoubleBuffer<unsigned int> d_values(d_colidx, d_colidx_alt);
    void *d_temp_storage = nullptr;
    size_t temp_storage_bytes = 0;
    cub::DeviceRadixSort::SortPairs(
        d_temp_storage,
        temp_storage_bytes,
        d_keys,
        d_values,
        n_elem);
    d_temp_storage = alloc.allocate(temp_storage_bytes);
    cub::DeviceRadixSort::SortPairs(
        d_temp_storage,
        temp_storage_bytes,
        d_keys,
        d_values,
        n_elem);
    alloc.deallocate((char *)d_temp_storage);

    thrust::device_ptr<unsigned int> rowidx(d_keys.Current());
    thrust::counting_iterator<unsigned int> rows_begin(0);
    thrust::device_ptr<unsigned int> csr_row_ptr(d_csr_row_ptr);
    thrust::lower_bound(
        thrust::cuda::par(alloc),
        rowidx,
        rowidx + n_elem,
        rows_begin,
        rows_begin + n_variables + 1,
        csr_row_ptr);

    // find connected components
    ecl_connected_components(
        n_variables,
        n_elem,
        (const int *) d_csr_row_ptr,
        (const int *) d_values.Current(),
        (int *) d_component_ptr,
        (int *) d_work,
        devprop,
        false);
    }

// solve the satisfiability problem
void solve_sat(
    unsigned int *d_watch,
    unsigned int *d_next_clause,
    unsigned int *d_head,
    unsigned int *d_next,
    unsigned int *d_h,
    unsigned int *d_state,
    const unsigned int maxn_literals,
    const unsigned int *d_literals,
    const unsigned int *d_n_literals,
    unsigned int *d_assignment,
    const unsigned int n_variables,
    unsigned int *d_unsat,
    const unsigned int *d_component_ptr,
    unsigned int *d_representative,
    unsigned int *d_heap,
    const unsigned int block_size)
    {
    hipMemsetAsync(d_unsat, 0, sizeof(unsigned int));
    hipMemsetAsync(d_heap, 0, sizeof(unsigned int));

    // initialize with sentinel values
    hipMemsetAsync(d_head, 0xff, sizeof(unsigned int)*n_variables);
    hipMemsetAsync(d_next, 0xff, sizeof(unsigned int)*n_variables);
    hipMemsetAsync(d_watch, 0xff, sizeof(unsigned int)*2*n_variables);
    hipMemsetAsync(d_next_clause, 0xff, sizeof(unsigned int)*n_variables*maxn_literals);

    hipLaunchKernelGGL(kernel::setup_watch_list, n_variables/block_size + 1, block_size, 0, 0,
        n_variables,
        maxn_literals,
        d_literals,
        d_n_literals,
        d_watch,
        d_next_clause);

    unsigned int sat_block_size = 256;
    hipLaunchKernelGGL(kernel::initialize_components, n_variables/sat_block_size + 1, sat_block_size, 0, 0,
        d_watch,
        d_assignment,
        d_component_ptr,
        n_variables,
        d_representative,
        d_head,
        d_next);

    hipLaunchKernelGGL(kernel::solve_sat, n_variables/sat_block_size + 1, sat_block_size, 0, 0,
        d_watch,
        d_next_clause,
        d_next,
        d_h,
        d_head,
        d_literals,
        d_assignment,
        d_state,
        d_representative,
        n_variables,
        d_unsat,
        d_heap);
    }

} //end namespace gpu
} //end namespace hpm

#undef check_cusparse