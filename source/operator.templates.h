#include <cache/operator.h>

#include <deal.II/dofs/dof_tools.h>

//////////////////////// OPERATOR ////////////////////////////
template <int dim>
Operator<dim>::
Operator(OperatorParameters<dim> const & parameters) 
  : dof_handler(*(parameters.dof_handler))
  , constraint_matrix(*(parameters.constraint_matrix))
  , mp_values(*(parameters.mp_values))
  , bp_values(*(parameters.bp_values))
{
    this->stiffness_matrix.reinit(*(parameters.sparsity_pattern));
    this->mass_matrix.reinit(*(parameters.sparsity_pattern));
    this->load_vector.reinit(*(parameters.some_vector));
}

template <int dim>
void
Operator<dim>::
set_null_space(unsigned int const               component,
               dealii::types::material_id const material_id) 
{
    unsigned int const n_components = dealii::DoFTools::n_components(this->dof_handler);
    std::vector<bool> mask(n_components, false);
    mask[component] = true;
    dealii::ComponentMask component_mask(mask);
    std::vector<bool> selected_dofs(this->dof_handler.n_dofs());
    dealii::DoFTools::extract_dofs(this->dof_handler, component_mask, selected_dofs);
    dealii::FiniteElement<dim> const & fe = this->dof_handler.get_fe();
    unsigned int const dofs_per_cell = fe.dofs_per_cell;
    std::vector<dealii::types::global_dof_index> local_dof_indices(dofs_per_cell);
    typename dealii::DoFHandler<dim>::active_cell_iterator
        cell = this->dof_handler.begin_active(),
        end_cell = this->dof_handler.end();
    for ( ; cell != end_cell; ++cell) {
        cell->get_dof_indices(local_dof_indices);
        if (cell->material_id() != material_id) {
            for (unsigned int dof = 0; dof < dofs_per_cell; ++dof) {
                selected_dofs[local_dof_indices[dof]] = false;
            } // end for dof
        } // end if
    } // end for cell
    for (dealii::types::global_dof_index dof = 0; dof < this->dof_handler.n_dofs(); ++dof) {
        if (selected_dofs[dof]) {
            this->null_space_dof_indices.push_back(dof);
        } // end if
    } // end for dof
    std::cout<<"dofs = "<<this->dof_handler.n_dofs()<<" - "<<this->null_space_dof_indices.size()<<"\n";
}