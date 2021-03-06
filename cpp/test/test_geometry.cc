/* Copyright (c) 2016, the Cap authors.
 *
 * This file is subject to the Modified BSD License and may not be distributed
 * without copyright and license information. Please refer to the file LICENSE
 * for the text and further information on this license.
 */

#define BOOST_TEST_MODULE TestGeometry

#include "main.cc"

#include <cap/energy_storage_device.h>
#include <cap/supercapacitor.h>
#include <cap/utils.h>
#include <cap/geometry.h>
#include <deal.II/grid/grid_out.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/info_parser.hpp>
#include <boost/test/unit_test.hpp>
#include <fstream>
#include <unordered_map>

// - Check that a mesh can be loaded, that the areas are computed correctly, and
// check that a mesh can be written.
// - Check that we can build a 3D geometry.

template <int dim>
void write_mesh(std::string const &mesh_file,
                std::shared_ptr<dealii::distributed::Triangulation<dim> const>
                    triangulation)
{
  dealii::GridOut mesh_writer;
  std::fstream fout;
  fout.open(mesh_file.c_str(), std::fstream::out);
  mesh_writer.write_vtu(*triangulation, fout);
  fout.close();
}

BOOST_AUTO_TEST_CASE(test_reset_geometry)
{
  std::shared_ptr<boost::property_tree::ptree> params(
      new boost::property_tree::ptree);
  params->put("anode_collector_material_id", 4);
  params->put("anode_electrode_material_id", 1);
  params->put("separator_material_id", 2);
  params->put("cathode_electrode_material_id", 3);
  params->put("cathode_collector_material_id", 5);
  params->put("type", "file");
  params->put("mesh_file", "mesh_2d.ucd");
  params->put("materials", 1);
  params->put("material_0.name", "all");
  params->put("material_0.material_id", "1,2,3,4,5");
  params->put("boundary_values.anode_boundary_id", "1");
  params->put("boundary_values.cathode_boundary_id", "2");

  cap::Geometry<2> geo(params, boost::mpi::communicator());
  write_mesh("output_test_geometry_0.vtu", geo.get_triangulation());

  dealii::distributed::Triangulation<2> const &tria = *geo.get_triangulation();
  std::cout << "cells=" << tria.n_active_cells() << "  "
            << "faces=" << tria.n_active_faces() << "  "
            << "vertices=" << tria.n_used_vertices() << "\n";

  std::unordered_map<dealii::types::material_id, double> measure;
  auto cell = tria.begin_active();
  auto end_cell = tria.end();
  double m(0.);
  for (; cell != end_cell; ++cell)
    if (cell->is_locally_owned())
    {
      measure[cell->material_id()] += cell->measure();
      m += cell->measure();
    }

  for (auto x : measure)
    std::cout << std::to_string(x.first) << "  " << x.second << "\n";

  double const percent_tolerance = 1.0e-12;
  std::array<double, 5> reference_measure = {6.25e-10, 1.25e-9, 1.25e-9,
                                             1.5e-10, 1.5e-10};
  unsigned int pos(0);
  for (std::string const layer :
       {"separator", "anode_electrode", "cathode_electrode", "anode_collector",
        "cathode_collector"})
  {
    BOOST_CHECK_CLOSE(measure[params->get<dealii::types::material_id>(
                          layer + "_material_id")],
                      reference_measure[pos], percent_tolerance);
    ++pos;
  }
}

BOOST_AUTO_TEST_CASE(test_throw_geometry)
{
  std::shared_ptr<boost::property_tree::ptree> params(
      new boost::property_tree::ptree);
  params->put("type", "supercapacitor");
  params->put("anode_collector_thickness", 5.0e-4);
  params->put("anode_electrode_thickness", 50.0e-4);
  params->put("separator_thickness", 25.0e-4);
  params->put("cathode_electrode_thickness", 50.0e-4);
  params->put("cathode_collector_thickness", 50.0e-4);
  params->put("geometric_area", 25.0e-2);
  params->put("tab_height", 5.0e-4);

  // For now, both collectors must have the same dimensions.
  BOOST_CHECK_THROW(cap::Geometry<2> geo(params, boost::mpi::communicator()),
                    std::runtime_error);
}

BOOST_AUTO_TEST_CASE(test_3d_geometry)
{
  boost::property_tree::ptree device_database;
  boost::property_tree::info_parser::read_info("super_capacitor.info",
                                               device_database);
  boost::property_tree::ptree geometry_database;
  boost::property_tree::info_parser::read_info("generate_mesh.info",
                                               geometry_database);
  std::vector<unsigned int> divisions(3);
  // Collector
  divisions[0] = 3;
  divisions[1] = 3;
  divisions[2] = 3;
  geometry_database.put("collector.divisions", cap::to_string(divisions));
  // Anode
  divisions[0] = 3;
  divisions[1] = 3;
  divisions[2] = 2;
  geometry_database.put("anode.divisions", cap::to_string(divisions));
  // Separator
  divisions[0] = 3;
  divisions[1] = 3;
  divisions[2] = 2;
  geometry_database.put("separator.divisions", cap::to_string(divisions));
  // Cathode
  divisions[0] = 3;
  divisions[1] = 3;
  divisions[2] = 2;
  geometry_database.put("cathode.divisions", cap::to_string(divisions));

  device_database.put_child("geometry", geometry_database);
  device_database.put("dim", 3);

  std::shared_ptr<cap::EnergyStorageDevice> device =
      cap::EnergyStorageDevice::build(device_database,
                                      boost::mpi::communicator());
  std::shared_ptr<cap::SuperCapacitor<3>> supercapacitor =
      std::static_pointer_cast<cap::SuperCapacitor<3>>(device);
  std::shared_ptr<cap::Geometry<3>> geometry = supercapacitor->get_geometry();
  std::shared_ptr<dealii::distributed::Triangulation<3> const> triangulation =
      geometry->get_triangulation();
  const unsigned int n_cells = 6912;
  BOOST_CHECK(n_cells == triangulation->n_active_cells());
}
