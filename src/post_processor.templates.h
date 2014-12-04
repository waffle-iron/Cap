#include <cap/post_processor.h>
#include <deal.II/fe/fe_values.h>
#include <deal.II/base/quadrature_lib.h>
#include <algorithm>
#include <functional>
#include <limits>

namespace cap {

//////////////////////// POSTPROCESSOR ////////////////////////////
template <int dim>
Postprocessor<dim>::
Postprocessor(std::shared_ptr<PostprocessorParameters<dim> const> parameters) 
  : dof_handler(*(parameters->dof_handler))
  , solution(*(parameters->solution))
  , mp_values(parameters->mp_values)
  , boundary_values(parameters->boundary_values)
{ }

template <int dim>
dealii::Vector<double> const &
Postprocessor<dim>::
get(std::string const & key) const
{
    throw std::runtime_error("Should have been overloaded..."); 
}

template <int dim>
void
Postprocessor<dim>::
get(std::string const & key, double & value) const
{
    std::unordered_map<std::string, double>::const_iterator it =
        this->values.find(key);
    AssertThrow(it != this->values.end(),
        dealii::StandardExceptions::ExcMessage("Key "+key+" doesn't exist"));
    value = it->second;
}

//////////////////////// SUPERCAPACITOR POSTPROCESSOR PARAMETERS ////////////////////////////
template <int dim>
SuperCapacitorPostprocessorParameters<dim>::
SuperCapacitorPostprocessorParameters(std::shared_ptr<boost::property_tree::ptree const> d)
    : PostprocessorParameters<dim>(d)
{ 
}

//////////////////////// SUPERCAPACITOR POSTPROCESSOR /////////////////////
template <int dim>
SuperCapacitorPostprocessor<dim>::
SuperCapacitorPostprocessor(std::shared_ptr<PostprocessorParameters<dim> const> parameters)
    : Postprocessor<dim>(parameters)
{
    this->values["max_temperature"] = 0.0;
    this->values["voltage"        ] = 0.0;
    this->values["current"        ] = 0.0;
    this->values["joule_heating"  ] = 0.0;
    this->values["surface_area"   ] = 0.0;
    this->values["volume"         ] = 0.0;
}

template <int dim>
void
SuperCapacitorPostprocessor<dim>::
reset(std::shared_ptr<PostprocessorParameters<dim> const> parameters)
{
    std::for_each(this->values.begin(), this->values.end(), 
        [] (std::unordered_map<std::string, double>::value_type & p) { p.second = 0.0; });
    this->values["max_temperature"] = -std::numeric_limits<double>::max();

    std::shared_ptr<boost::property_tree::ptree const> database = parameters->database;

    dealii::types::boundary_id const cathode_boundary_id = database->get<dealii::types::boundary_id>("boundary_values.cathode_boundary_id");
    dealii::FEValuesExtractors::Scalar const temperature     (database->get<unsigned int>("temperature_component"     ));
    dealii::FEValuesExtractors::Scalar const solid_potential (database->get<unsigned int>("solid_potential_component" ));
    dealii::FEValuesExtractors::Scalar const liquid_potential(database->get<unsigned int>("liquid_potential_component"));

    dealii::Triangulation<dim> const & triangulation = this->dof_handler.get_tria();
    dealii::Vector<double> joule_heating(triangulation.n_active_cells());

    dealii::FiniteElement<dim> const & fe = this->dof_handler.get_fe(); // TODO: don't want to use directly fe because we might create postprocessor that will only know about dof_handler
    dealii::QGauss<dim>   quadrature_rule     (fe.degree+1);
    dealii::QGauss<dim-1> face_quadrature_rule(fe.degree+1);
    dealii::FEValues<dim>     fe_values     (fe, quadrature_rule,      dealii::update_values | dealii::update_gradients | dealii::update_JxW_values);
    dealii::FEFaceValues<dim> fe_face_values(fe, face_quadrature_rule, dealii::update_values | dealii::update_gradients | dealii::update_JxW_values | dealii::update_normal_vectors);
//    unsigned int const dofs_per_cell   = fe.dofs_per_cell;
    unsigned int const n_q_points      = quadrature_rule     .size();
    unsigned int const n_face_q_points = face_quadrature_rule.size();
    std::vector<double>                  solid_electrical_conductivity_values (n_q_points);
    std::vector<double>                  liquid_electrical_conductivity_values(n_q_points);
    std::vector<dealii::Tensor<1, dim> > solid_potential_gradients            (n_q_points);
    std::vector<dealii::Tensor<1, dim> > liquid_potential_gradients           (n_q_points);
    std::vector<double>                  temperature_values                   (n_q_points);
    double                               max_temperature;

    std::vector<double>                  face_solid_electrical_conductivity_values(n_face_q_points);
    std::vector<double>                  face_solid_potential_values              (n_face_q_points);
    std::vector<dealii::Tensor<1, dim> > face_solid_potential_gradients           (n_face_q_points);
    std::vector<dealii::Point<dim> >     normal_vectors                           (n_face_q_points);

    typename dealii::DoFHandler<dim>::active_cell_iterator
        cell = this->dof_handler.begin_active(),
        end_cell = this->dof_handler.end();
    for ( ; cell != end_cell; ++cell) {
        fe_values.reinit(cell);
        this->mp_values->get_values("solid_electrical_conductivity",  cell, solid_electrical_conductivity_values );
        this->mp_values->get_values("liquid_electrical_conductivity", cell, liquid_electrical_conductivity_values);
        fe_values[solid_potential ].get_function_gradients(this->solution, solid_potential_gradients );
        fe_values[liquid_potential].get_function_gradients(this->solution, liquid_potential_gradients);
        fe_values[temperature]      .get_function_values  (this->solution, temperature_values        );
        for (unsigned int q_point = 0; q_point < n_q_points; ++q_point) {
            this->values["joule_heating"] += 
              ( solid_electrical_conductivity_values [q_point] * solid_potential_gradients [q_point] * solid_potential_gradients [q_point]
              + liquid_electrical_conductivity_values[q_point] * liquid_potential_gradients[q_point] * liquid_potential_gradients[q_point]
              ) * fe_values.JxW(q_point);
            this->values["volume"       ] += fe_values.JxW(q_point);
        } // end for quadrature point
        max_temperature = *std::max_element(temperature_values.begin(), temperature_values.end());
        if (max_temperature > this->values["max_temperature"]) {
             this->values["max_temperature"] = max_temperature;
        } // end if
        if (cell->at_boundary()) {
            for (unsigned int face = 0; face < dealii::GeometryInfo<dim>::faces_per_cell; ++face) {
                if (cell->face(face)->at_boundary()) {
if (cell->face(face)->boundary_indicator() == cathode_boundary_id) {
                    fe_face_values.reinit(cell, face);
                    this->mp_values->get_values("solid_electrical_conductivity", cell, face_solid_electrical_conductivity_values); // TODO: should take face as an argument...
                    fe_face_values[solid_potential].get_function_gradients(this->solution, face_solid_potential_gradients);
                    fe_face_values[solid_potential].get_function_values   (this->solution, face_solid_potential_values   );
                    normal_vectors = fe_face_values.get_normal_vectors();
                    for (unsigned int face_q_point = 0; face_q_point < n_face_q_points; ++face_q_point) {
                        this->values["current"     ] += 
                          ( face_solid_electrical_conductivity_values [face_q_point] * face_solid_potential_gradients[face_q_point] 
                          * normal_vectors[face_q_point] 
                          ) * fe_face_values.JxW(face_q_point); 
                        this->values["voltage"     ] += face_solid_potential_values[face_q_point] * fe_face_values.JxW(face_q_point);
                        this->values["surface_area"] += fe_face_values.JxW(face_q_point);
                    } // end for face quadrature point
} // end if cathode
                } // end if face at boundary
            } // end for face
        } // end if cell at boundary
    } // end for cell
    this->values["voltage"] /= this->values["surface_area"];
//    std::for_each(this->values.begin(), this->values.end(), [] (std::unordered_map<std::string, double>::value_type & p) { std::cout<<p.first<<"  "<<p.second<<"\n"; });
}

} // end namespace cap