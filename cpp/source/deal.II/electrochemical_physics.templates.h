/* Copyright (c) 2016, the Cap authors.
 *
 * This file is subject to the Modified BSD License and may not be distributed
 * without copyright and license information. Please refer to the file LICENSE
 * for the text and further information on this license.
 */

#ifndef CAP_DEAL_II_ELECTROCHEMICAL_PHYSICS_TEMPLATES_H
#define CAP_DEAL_II_ELECTROCHEMICAL_PHYSICS_TEMPLATES_H

#include <cap/electrochemical_physics.h>
#include <cap/types.h>
#include <boost/assert.hpp>
#include <deal.II/base/function.h>
#include <deal.II/dofs/dof_tools.h>
#include <deal.II/fe/fe_values.h>
#include <deal.II/numerics/vector_tools.h>

namespace cap
{
template <int dim>
ElectrochemicalPhysics<dim>::ElectrochemicalPhysics(
    std::shared_ptr<PhysicsParameters<dim> const> parameters,
    boost::mpi::communicator mpi_communicator)
    : Physics<dim>(parameters, mpi_communicator), solid_potential_component(-1),
      liquid_potential_component(-1),
      anode_boundary_id(type::invalid_boundary_id),
      cathode_boundary_id(type::invalid_boundary_id),
      _assembly_timer(mpi_communicator, "ElectrochemicalPhysics assembly"),
      _setup_timer(mpi_communicator, "ElectrochemicalPhysics setup")
{
  _setup_timer.start();
  boost::property_tree::ptree const &database = parameters->database;

  // clang-format off
  this->solid_potential_component  = database.get<unsigned int>("solid_potential_component");
  this->liquid_potential_component = database.get<unsigned int>("liquid_potential_component");
  // clang-format on

  anode_boundary_id = parameters->geometry->get_anode_boundary_id();
  cathode_boundary_id = parameters->geometry->get_cathode_boundary_id();

  std::shared_ptr<
      ElectrochemicalPhysicsParameters<dim> const> electrochemical_parameters =
      std::dynamic_pointer_cast<ElectrochemicalPhysicsParameters<dim> const>(
          parameters);
  BOOST_ASSERT_MSG(electrochemical_parameters != nullptr,
                   "Problem during dowcasting the pointer");

  // Initialize locally_owned_dofs and locally_relevant_dofs
  this->locally_owned_dofs = this->dof_handler->locally_owned_dofs();
  dealii::DoFTools::extract_locally_relevant_dofs(*(this->dof_handler),
                                                  this->locally_relevant_dofs);

  // Take care of hanging nodes
  this->constraint_matrix.clear();
  this->constraint_matrix.reinit(this->locally_relevant_dofs);
  dealii::DoFTools::make_hanging_node_constraints(*(this->dof_handler),
                                                  this->constraint_matrix);

  // Take care of Dirichlet boundary condition.
  // The anode is always set in Earth (Dirichlet value of 0).
  // If we impose a the voltage, the cathode is also a Dirichlet condition.
  unsigned int const n_components =
      dealii::DoFTools::n_components(*(this->dof_handler));
  std::vector<bool> mask(n_components, false);
  mask[this->solid_potential_component] = true;
  dealii::ComponentMask component_mask(mask);
  typename dealii::FunctionMap<dim>::type dirichlet_boundary_condition;
  dealii::ZeroFunction<dim> homogeneous_bc(n_components);
  dirichlet_boundary_condition[anode_boundary_id] = &homogeneous_bc;
  std::unique_ptr<dealii::Function<dim>> cathode_dirichlet_bc = nullptr;
  bool inhomogeneous_bc = false;
  if (electrochemical_parameters->supercapacitor_state == ConstantVoltage)
  {
    cathode_dirichlet_bc = std::make_unique<dealii::ConstantFunction<dim>>(
        dealii::ConstantFunction<dim>(
            electrochemical_parameters->constant_voltage, n_components));
    dirichlet_boundary_condition[cathode_boundary_id] =
        cathode_dirichlet_bc.get();
    inhomogeneous_bc = true;
  }

  dealii::VectorTools::interpolate_boundary_values(
      *(this->dof_handler), dirichlet_boundary_condition,
      this->constraint_matrix, component_mask);

  // Finally close the ConstraintMatrix.
  this->constraint_matrix.close();

  // Create sparsity pattern
  this->sparsity_pattern.reinit(
      this->locally_owned_dofs, this->locally_owned_dofs,
      this->locally_relevant_dofs, this->mpi_communicator);
  dealii::DoFTools::make_sparsity_pattern(
      *(this->dof_handler), this->sparsity_pattern, this->constraint_matrix,
      true, dealii::Utilities::MPI::this_mpi_process(this->mpi_communicator));
  this->sparsity_pattern.compress();

  // Initialize matrices and vectors
  this->system_matrix.reinit(this->sparsity_pattern);
  this->mass_matrix.reinit(this->sparsity_pattern);
  this->system_rhs.reinit(this->locally_owned_dofs, this->mpi_communicator);

  _setup_timer.stop();
  assemble_system(parameters, inhomogeneous_bc);
}

template <int dim>
ElectrochemicalPhysics<dim>::~ElectrochemicalPhysics()
{
  if (this->verbose_lvl > 0)
  {
    _setup_timer.print();
    _assembly_timer.print();
  }
}

template <int dim>
void ElectrochemicalPhysics<dim>::assemble_system(
    std::shared_ptr<PhysicsParameters<dim> const> parameters,
    bool const inhomogeneous_bc)
{
  _assembly_timer.start();
  std::shared_ptr<
      ElectrochemicalPhysicsParameters<dim> const> electrochemical_parameters =
      std::dynamic_pointer_cast<ElectrochemicalPhysicsParameters<dim> const>(
          parameters);
  BOOST_ASSERT_MSG(electrochemical_parameters != nullptr,
                   "Problem during dowcasting the pointer");

  dealii::DoFHandler<dim> const &dof_handler = *(this->dof_handler);
  dealii::FiniteElement<dim> const &fe = dof_handler.get_fe();

  dealii::FEValuesExtractors::Scalar const solid_potential(
      this->solid_potential_component);
  dealii::FEValuesExtractors::Scalar const liquid_potential(
      this->liquid_potential_component);
  dealii::QGauss<dim> quadrature_rule(fe.degree + 1);
  dealii::FEValues<dim> fe_values(
      fe, quadrature_rule, dealii::update_values | dealii::update_gradients |
                               dealii::update_JxW_values);

  unsigned int const dofs_per_cell = fe.dofs_per_cell;
  unsigned int const n_q_points = quadrature_rule.size();
  double const time_step = electrochemical_parameters->time_step;
  dealii::Vector<double> cell_rhs(dofs_per_cell);
  dealii::FullMatrix<double> cell_system_matrix(dofs_per_cell, dofs_per_cell);
  dealii::FullMatrix<double> cell_mass_matrix(dofs_per_cell, dofs_per_cell);
  std::vector<double> solid_phase_diffusion_coefficient_values(n_q_points);
  std::vector<double> liquid_phase_diffusion_coefficient_values(n_q_points);
  std::vector<double> specific_capacitance_values(n_q_points);
  std::vector<double> faradaic_reaction_coefficient_values(n_q_points);
  std::vector<dealii::types::global_dof_index> local_dof_indices(dofs_per_cell);

  this->system_matrix = 0.0;
  this->mass_matrix = 0.0;
  this->system_rhs = 0.0;

  for (auto cell : dof_handler.active_cell_iterators())
  {
    if (cell->is_locally_owned())
    {
      cell_system_matrix = 0.0;
      cell_mass_matrix = 0.0;
      cell_rhs = 0.0;
      fe_values.reinit(cell);

      // clang-format off
      (this->mp_values)->get_values("specific_capacitance",           cell, specific_capacitance_values);
      (this->mp_values)->get_values("solid_electrical_conductivity",  cell, solid_phase_diffusion_coefficient_values);
      (this->mp_values)->get_values("liquid_electrical_conductivity", cell, liquid_phase_diffusion_coefficient_values);
      (this->mp_values)->get_values("faradaic_reaction_coefficient",  cell, faradaic_reaction_coefficient_values);
      // clang-format on

      // The coefficients are zeros when the physics does not make sense.
      for (unsigned int q = 0; q < n_q_points; ++q)
        for (unsigned int i = 0; i < dofs_per_cell; ++i)
        {
          for (unsigned int j = 0; j < dofs_per_cell; ++j)
          {
            // Mass matrix terms
            double const mass_matrix_val =
                specific_capacitance_values[q] *
                (fe_values[solid_potential].value(i, q) *
                     fe_values[solid_potential].value(j, q) -
                 fe_values[solid_potential].value(i, q) *
                     fe_values[liquid_potential].value(j, q) -
                 fe_values[liquid_potential].value(i, q) *
                     fe_values[solid_potential].value(j, q) +
                 fe_values[liquid_potential].value(i, q) *
                     fe_values[liquid_potential].value(j, q)) *
                fe_values.JxW(q);
            cell_mass_matrix(i, j) += mass_matrix_val;
            cell_system_matrix(i, j) +=
                mass_matrix_val +
                time_step *
                    // Stiffness matrix terms
                    (solid_phase_diffusion_coefficient_values[q] *
                         (fe_values[solid_potential].gradient(i, q) *
                          fe_values[solid_potential].gradient(j, q)) +
                     liquid_phase_diffusion_coefficient_values[q] *
                         (fe_values[liquid_potential].gradient(i, q) *
                          fe_values[liquid_potential].gradient(j, q)) +
                     faradaic_reaction_coefficient_values[q] *
                         ((fe_values[solid_potential].value(i, q) *
                           fe_values[solid_potential].value(j, q)) -
                          (fe_values[liquid_potential].value(i, q) *
                           fe_values[solid_potential].value(j, q)) -
                          (fe_values[solid_potential].value(i, q) *
                           fe_values[liquid_potential].value(j, q)) +
                          (fe_values[liquid_potential].value(i, q) *
                           fe_values[liquid_potential].value(j, q)))) *
                    fe_values.JxW(q);
          }
        }

      // Fill in the global matrices.
      cell->get_dof_indices(local_dof_indices);
      this->constraint_matrix.distribute_local_to_global(
          cell_system_matrix, cell_rhs, local_dof_indices, this->system_matrix,
          this->system_rhs, inhomogeneous_bc);
      for (unsigned int i = 0; i < dofs_per_cell; ++i)
        for (unsigned int j = 0; j < dofs_per_cell; ++j)
          this->mass_matrix.add(local_dof_indices[i], local_dof_indices[j],
                                cell_mass_matrix(i, j));
    }
  }

  // Apply Neumann boundary condition on the cathode (constant current
  // charge).
  if (electrochemical_parameters->supercapacitor_state == ConstantCurrent)
  {
    double const time_step = electrochemical_parameters->time_step;
    double const current_density =
        electrochemical_parameters->constant_current_density;
    dealii::QGauss<dim - 1> face_quadrature_rule(fe.degree + 1);
    unsigned int const n_face_q_points = face_quadrature_rule.size();
    dealii::FEFaceValues<dim> fe_face_values(fe, face_quadrature_rule,
                                             dealii::update_values |
                                                 dealii::update_JxW_values);
    for (auto cell : dof_handler.active_cell_iterators())
      if (cell->is_locally_owned() && cell->at_boundary())
      {
        cell_rhs = 0.0;
        for (unsigned int face = 0;
             face < dealii::GeometryInfo<dim>::faces_per_cell; ++face)
        {
          if ((cell->face(face)->at_boundary()) &&
              (cell->face(face)->boundary_id() == cathode_boundary_id))
          {
            fe_face_values.reinit(cell, face);
            for (unsigned int q = 0; q < n_face_q_points; ++q)
              for (unsigned int i = 0; i < dofs_per_cell; ++i)
                cell_rhs[i] += time_step * current_density *
                               fe_face_values[solid_potential].value(i, q) *
                               fe_face_values.JxW(q);
          }
        }
        cell->get_dof_indices(local_dof_indices);
        this->constraint_matrix.distribute_local_to_global(
            cell_rhs, local_dof_indices, this->system_rhs);
      }
  }

  // We are done fill-in the matrices and the vector. So we can compress
  // everything.
  this->system_matrix.compress(dealii::VectorOperation::add);
  this->mass_matrix.compress(dealii::VectorOperation::add);
  this->system_rhs.compress(dealii::VectorOperation::add);

  _assembly_timer.stop();
}
}

#endif
