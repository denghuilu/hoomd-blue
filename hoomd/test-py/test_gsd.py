# -*- coding: iso-8859-1 -*-
# Maintainer: joaander

from hoomd import *
import hoomd;
context.initialize()
import unittest
import os
import numpy

# unit tests for dump.gsd
class gsd_write_tests (unittest.TestCase):
    def setUp(self):
        print
        self.snapshot = data.make_snapshot(N=4, box=data.boxdim(Lx=10, Ly=20, Lz=30), dtype='float');
        if comm.get_rank() == 0:
            # particles
            self.snapshot.particles.position[0] = [0,1,2];
            self.snapshot.particles.position[1] = [1,2,3];
            self.snapshot.particles.position[2] = [0,-1,-2];
            self.snapshot.particles.position[3] = [-1, -2, -3];
            self.snapshot.particles.velocity[0] = [10, 11, 12];
            self.snapshot.particles.velocity[1] = [11, 12, 13];
            self.snapshot.particles.velocity[2] = [12, 13, 14];
            self.snapshot.particles.velocity[3] = [13, 14, 15];
            self.snapshot.particles.acceleration[0] = [20, 21, 22];
            self.snapshot.particles.acceleration[1] = [21, 22, 23];
            self.snapshot.particles.acceleration[2] = [22, 23, 24];
            self.snapshot.particles.acceleration[3] = [23, 24, 25];
            self.snapshot.particles.typeid[:] = [0,0,1,1];
            self.snapshot.particles.mass[:] = [33, 34, 35,  36];
            self.snapshot.particles.charge[:] = [44, 45, 46, 47];
            self.snapshot.particles.diameter[:] = [55, 56, 57, 58];
            self.snapshot.particles.image[0] = [60, 61, 62];
            self.snapshot.particles.image[1] = [61, 62, 63];
            self.snapshot.particles.image[2] = [62, 63, 64];
            self.snapshot.particles.image[3] = [63, 64, 65];
            self.snapshot.particles.types = ['p1', 'p2'];

            # bonds
            self.snapshot.bonds.types = ['b1', 'b2'];
            self.snapshot.bonds.resize(2);
            self.snapshot.bonds.typeid[:] = [0, 1];
            self.snapshot.bonds.group[0] = [0, 1];
            self.snapshot.bonds.group[1] = [2, 3];

            # angles
            self.snapshot.angles.types = ['a1', 'a2'];
            self.snapshot.angles.resize(2);
            self.snapshot.angles.typeid[:] = [1, 0];
            self.snapshot.angles.group[0] = [0, 1, 2];
            self.snapshot.angles.group[1] = [2, 3, 0];

            # dihedrals
            self.snapshot.dihedrals.types = ['d1'];
            self.snapshot.dihedrals.resize(1);
            self.snapshot.dihedrals.typeid[:] = [0];
            self.snapshot.dihedrals.group[0] = [0, 1, 2, 3];

            # impropers
            self.snapshot.impropers.types = ['i1'];
            self.snapshot.impropers.resize(1);
            self.snapshot.impropers.typeid[:] = [0];
            self.snapshot.impropers.group[0] = [3, 2, 1, 0];

            # constraints
            self.snapshot.constraints.resize(1)
            self.snapshot.constraints.group[0] = [0, 1]
            self.snapshot.constraints.value[0] = 2.5

        self.s = init.read_snapshot(self.snapshot);

        context.current.sorter.set_params(grid=8)

    # tests basic creation of the dump
    def test(self):
        dump.gsd(filename="test.gsd", group=group.all(), period=1, overwrite=True);
        run(5);
        # ensure 5 frames are written to the file
        data.gsd_snapshot('test.gsd', frame=4);
        if comm.get_rank() == 0:
            self.assertRaises(RuntimeError, data.gsd_snapshot, 'test.gsd', frame=5);

    # tests with phase
    def test_phase(self):
        dump.gsd(filename="test.gsd", group=group.all(), period=1, phase=0, overwrite=True);
        run(1);
        data.gsd_snapshot('test.gsd', frame=0);
        if comm.get_rank() == 0:
            self.assertRaises(RuntimeError, data.gsd_snapshot, 'test.gsd', frame=1);

    # tests overwrite
    def test_overwrite(self):
        if comm.get_rank() == 0:
            with open('test.gsd', 'wt') as f:
                f.write('Hello');

        dump.gsd(filename="test.gsd", group=group.all(), period=1, overwrite=True);
        run(1);
        data.gsd_snapshot('test.gsd', frame=0);
        if comm.get_rank() == 0:
            self.assertRaises(RuntimeError, data.gsd_snapshot, 'test.gsd', frame=1);

    # tests truncate
    def test_truncate(self):
        dump.gsd(filename="test.gsd", group=group.all(), period=1, truncate=True, overwrite=True);
        run(5);
        data.gsd_snapshot('test.gsd', frame=0);
        if comm.get_rank() == 0:
            self.assertRaises(RuntimeError, data.gsd_snapshot, 'test.gsd', frame=1);

    # tests write_restart
    def write_restart(self):
        g = dump.gsd(filename="test.gsd", group=group.all(), period=1000000, truncate=True, overwrite=True);
        run(5);
        g.write_restart();
        data.gsd_snapshot('test.gsd', frame=0);
        if comm.get_rank() == 0:
            self.assertRaises(RuntimeError, data.gsd_snapshot, 'test.gsd', frame=1);

    # test all static quantities
    def test_all_static(self):
        dump.gsd(filename="test.gsd", group=group.all(), period=1, static=['attribute', 'property', 'momentum', 'topology'], overwrite=True);
        run(1);
        data.gsd_snapshot('test.gsd', frame=0);
        if comm.get_rank() == 0:
            self.assertRaises(RuntimeError, data.gsd_snapshot, 'test.gsd', frame=1);

    # test write file
    def test_write_immediate(self):
        dump.gsd(filename="test.gsd", group=group.all(), period=None, time_step=1000, overwrite=True);
        data.gsd_snapshot('test.gsd', frame=0);
        if comm.get_rank() == 0:
            self.assertRaises(RuntimeError, data.gsd_snapshot, 'test.gsd', frame=1);

    # tests init.read_gsd
    def test_read_gsd(self):
        dump.gsd(filename="test.gsd", group=group.all(), period=1, overwrite=True);
        run(5);

        context.initialize();
        init.read_gsd(filename='test.gsd', frame=4);
        if comm.get_rank() == 0:
            self.assertRaises(RuntimeError, init.read_gsd, 'test.gsd', frame=5);


    def tearDown(self):
        if comm.get_rank() == 0:
            os.remove('test.gsd');

        comm.barrier_all();
        context.initialize();

# unit tests for dump.gsd
class gsd_read_tests (unittest.TestCase):
    def setUp(self):
        print
        self.snapshot = data.make_snapshot(N=4, box=data.boxdim(L=10), dtype='float');
        if comm.get_rank() == 0:
            # particles
            self.snapshot.particles.position[0] = [0,1,2];
            self.snapshot.particles.position[1] = [1,2,3];
            self.snapshot.particles.position[2] = [0,-1,-2];
            self.snapshot.particles.position[3] = [-1, -2, -3];
            self.snapshot.particles.velocity[0] = [10, 11, 12];
            self.snapshot.particles.velocity[1] = [11, 12, 13];
            self.snapshot.particles.velocity[2] = [12, 13, 14];
            self.snapshot.particles.velocity[3] = [13, 14, 15];
            self.snapshot.particles.orientation[0] = [19, 20, 21, 22];
            self.snapshot.particles.orientation[1] = [20, 21, 22, 23];
            self.snapshot.particles.orientation[2] = [21, 22, 23, 24];
            self.snapshot.particles.orientation[3] = [22, 23, 24, 25];
            self.snapshot.particles.angmom[0] = [119, 220, 321, 422];
            self.snapshot.particles.angmom[1] = [120, 221, 322, 423];
            self.snapshot.particles.angmom[2] = [121, 222, 323, 424];
            self.snapshot.particles.angmom[3] = [122, 223, 324, 425];
            self.snapshot.particles.typeid[:] = [0,0,1,1];
            self.snapshot.particles.mass[:] = [33, 34, 35,  36];
            self.snapshot.particles.charge[:] = [44, 45, 46, 47];
            self.snapshot.particles.diameter[:] = [55, 56, 57, 58];
            self.snapshot.particles.body[:] = [-1, -1, -1, -1];
            self.snapshot.particles.image[0] = [60, 61, 62];
            self.snapshot.particles.image[1] = [61, 62, 63];
            self.snapshot.particles.image[2] = [62, 63, 64];
            self.snapshot.particles.image[3] = [63, 64, 65];
            self.snapshot.particles.moment_inertia[0] = [50, 51, 52];
            self.snapshot.particles.moment_inertia[1] = [51, 52, 53];
            self.snapshot.particles.moment_inertia[2] = [52, 53, 54];
            self.snapshot.particles.moment_inertia[3] = [53, 54, 55];
            self.snapshot.particles.types = ['p1', 'p2'];

            # bonds
            self.snapshot.bonds.types = ['b1', 'b2'];
            self.snapshot.bonds.resize(2);
            self.snapshot.bonds.typeid[:] = [0, 1];
            self.snapshot.bonds.group[0] = [0, 1];
            self.snapshot.bonds.group[1] = [2, 3];

            # angles
            self.snapshot.angles.types = ['a1', 'a2'];
            self.snapshot.angles.resize(2);
            self.snapshot.angles.typeid[:] = [1, 0];
            self.snapshot.angles.group[0] = [0, 1, 2];
            self.snapshot.angles.group[1] = [2, 3, 0];

            # dihedrals
            self.snapshot.dihedrals.types = ['d1'];
            self.snapshot.dihedrals.resize(1);
            self.snapshot.dihedrals.typeid[:] = [0];
            self.snapshot.dihedrals.group[0] = [0, 1, 2, 3];

            # impropers
            self.snapshot.impropers.types = ['i1'];
            self.snapshot.impropers.resize(1);
            self.snapshot.impropers.typeid[:] = [0];
            self.snapshot.impropers.group[0] = [3, 2, 1, 0];

            # constraints
            self.snapshot.constraints.resize(1)
            self.snapshot.constraints.group[0] = [0, 1]
            self.snapshot.constraints.value[0] = 2.5

        self.s = init.read_snapshot(self.snapshot);
        context.current.sorter.set_params(grid=8)

    # tests data.gsd_snapshot
    def test_gsd_snapshot(self):
        dump.gsd(filename="test.gsd", group=group.all(), period=None, overwrite=True);

        snap = data.gsd_snapshot('test.gsd', frame=0);
        if comm.get_rank() == 0:
            self.assertEqual(snap.box.dimensions, self.snapshot.box.dimensions);
            self.assertEqual(snap.box.Lx, self.snapshot.box.Lx);
            self.assertEqual(snap.box.Ly, self.snapshot.box.Ly);
            self.assertEqual(snap.box.Lz, self.snapshot.box.Lz);
            self.assertEqual(snap.box.xy, self.snapshot.box.xy);
            self.assertEqual(snap.box.xz, self.snapshot.box.xz);
            self.assertEqual(snap.box.yz, self.snapshot.box.yz);

            self.assertEqual(snap.particles.N, self.snapshot.particles.N);
            self.assertEqual(snap.particles.types, self.snapshot.particles.types);

            numpy.testing.assert_array_equal(snap.particles.typeid, self.snapshot.particles.typeid);
            numpy.testing.assert_array_equal(snap.particles.mass, self.snapshot.particles.mass);
            numpy.testing.assert_array_equal(snap.particles.charge, self.snapshot.particles.charge);
            numpy.testing.assert_array_equal(snap.particles.diameter, self.snapshot.particles.diameter);
            numpy.testing.assert_array_equal(snap.particles.body, self.snapshot.particles.body);
            numpy.testing.assert_array_equal(snap.particles.moment_inertia, self.snapshot.particles.moment_inertia);
            numpy.testing.assert_array_equal(snap.particles.position, self.snapshot.particles.position);
            numpy.testing.assert_array_equal(snap.particles.orientation, self.snapshot.particles.orientation);
            numpy.testing.assert_array_equal(snap.particles.velocity, self.snapshot.particles.velocity);
            numpy.testing.assert_array_equal(snap.particles.angmom, self.snapshot.particles.angmom);
            numpy.testing.assert_array_equal(snap.particles.image, self.snapshot.particles.image);

            self.assertEqual(snap.bonds.N, self.snapshot.bonds.N);
            self.assertEqual(snap.bonds.types, self.snapshot.bonds.types);
            numpy.testing.assert_array_equal(snap.bonds.typeid, self.snapshot.bonds.typeid);
            numpy.testing.assert_array_equal(snap.bonds.group, self.snapshot.bonds.group);

            self.assertEqual(snap.angles.N, self.snapshot.angles.N);
            self.assertEqual(snap.angles.types, self.snapshot.angles.types);
            numpy.testing.assert_array_equal(snap.angles.typeid, self.snapshot.angles.typeid);
            numpy.testing.assert_array_equal(snap.angles.group, self.snapshot.angles.group);

            self.assertEqual(snap.dihedrals.N, self.snapshot.dihedrals.N);
            self.assertEqual(snap.dihedrals.types, self.snapshot.dihedrals.types);
            numpy.testing.assert_array_equal(snap.dihedrals.typeid, self.snapshot.dihedrals.typeid);
            numpy.testing.assert_array_equal(snap.dihedrals.group, self.snapshot.dihedrals.group);

            self.assertEqual(snap.impropers.N, self.snapshot.impropers.N);
            self.assertEqual(snap.impropers.types, self.snapshot.impropers.types);
            numpy.testing.assert_array_equal(snap.impropers.typeid, self.snapshot.impropers.typeid);
            numpy.testing.assert_array_equal(snap.impropers.group, self.snapshot.impropers.group);

            self.assertEqual(snap.constraints.N, self.snapshot.constraints.N);
            numpy.testing.assert_array_equal(snap.constraints.group, self.snapshot.constraints.group);
            numpy.testing.assert_array_equal(snap.constraints.value, self.snapshot.constraints.value);

    # tests init.read_gsd
    def test_read_gsd(self):
        dump.gsd(filename="test.gsd", group=group.all(), period=None, overwrite=True);
        context.initialize();

        init.read_gsd(filename='test.gsd');

    def tearDown(self):
        if comm.get_rank() == 0:
            os.remove('test.gsd');
        comm.barrier_all();
        context.initialize();

# unit tests for dump.gsd with default type
class gsd_default_type (unittest.TestCase):
    def setUp(self):
        print
        self.snapshot = data.make_snapshot(N=4, box=data.boxdim(L=10), dtype='float');
        if comm.get_rank() == 0:
            # particles
            self.snapshot.particles.position[0] = [0,1,2];
            self.snapshot.particles.position[1] = [1,2,3];
            self.snapshot.particles.position[2] = [0,-1,-2];
            self.snapshot.particles.position[3] = [-1, -2, -3];
            self.snapshot.particles.velocity[0] = [10, 11, 12];
            self.snapshot.particles.velocity[1] = [11, 12, 13];
            self.snapshot.particles.velocity[2] = [12, 13, 14];
            self.snapshot.particles.velocity[3] = [13, 14, 15];
            self.snapshot.particles.types = ['A'];

        self.s = init.read_snapshot(self.snapshot);
        context.current.sorter.set_params(grid=8)

    # tests data.gsd_snapshot
    def test_gsd(self):
        dump.gsd(filename="test.gsd", group=group.all(), period=None, overwrite=True);

        snap = data.gsd_snapshot('test.gsd', frame=0);
        if comm.get_rank() == 0:
            self.assertEqual(snap.particles.N, self.snapshot.particles.N);
            self.assertEqual(snap.particles.types, self.snapshot.particles.types);

    # tests init.read_gsd
    def test_read_gsd(self):
        dump.gsd(filename="test.gsd", group=group.all(), period=None, overwrite=True);
        context.initialize();

        init.read_gsd(filename='test.gsd');

    def tearDown(self):
        if comm.get_rank() == 0:
            os.remove('test.gsd');
        comm.barrier_all();
        context.initialize();


if __name__ == '__main__':
    unittest.main(argv = ['test.py', '-v'])
