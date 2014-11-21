#include <cache/super_capacitor.h>
#include <cache/utils.h>
#include <boost/property_tree/ptree.hpp>
#include <boost/foreach.hpp>
#include <iostream>
#include <string>
#include <vector>

void put_default_parameters(boost::property_tree::ptree & params)
{
    params.put("material_properties.separator_thermal_conductivity", 1.2528e3);
    params.put("material_properties.electrode_thermal_conductivity", 0.93e3);
    params.put("material_properties.collector_thermal_conductivity", 2.7e3);
    params.put("material_properties.separator_density",              3.1404e3);             
    params.put("material_properties.electrode_density",              1.34e3);             
    params.put("material_properties.collector_density",              0.89815e3);             
    params.put("material_properties.separator_heat_capacity",        0.0019e2);       
    params.put("material_properties.electrode_heat_capacity",        0.0011e2);       
    params.put("material_properties.collector_heat_capacity",        2.37e2);       
                                                                     
    params.put("material_properties.specific_capacitance",           86.0e6);          
    params.put("material_properties.separator_void_volume_fraction", 0.6);
    params.put("material_properties.electrode_void_volume_fraction", 0.67);
    params.put("material_properties.electrolyte_conductivity",       0.067);      
    params.put("material_properties.solid_phase_conductivity",       52.1);      

    params.put("boundary_values.charge_potential",             2.2);      
    params.put("boundary_values.discharge_potential",          1.4);      
    params.put("boundary_values.charge_current_density",     324.65);      
    params.put("boundary_values.discharge_current_density", -324.65);      
    params.put("boundary_values.initial_potential",            1.6);      
                                                                  
    params.put("boundary_values.upper_ambient_temperature",       0.0);      
    params.put("boundary_values.lower_ambient_temperature",       0.0);      
    params.put("boundary_values.upper_heat_transfer_coefficient", 8.0e-2);      
    params.put("boundary_values.lower_heat_transfer_coefficient", 0.0);      

    params.put("time_step",     "0.1");
    params.put("initial_time",  "0.0");
    params.put("final_time",   "30.0");
}

int main(int argc, char *argv[])
{
    std::cout<<"hello world\n";

    std::shared_ptr<boost::property_tree::ptree> database(new boost::property_tree::ptree);
    put_default_parameters(*database); // TODO: only temporary
    cache::SuperCapacitorProblemParameters params(database);
    cache::SuperCapacitorProblem<2> super_capacitor(params);

    // SETTING PROBLEM PARAMETERS
    std::shared_ptr<boost::property_tree::ptree> in(new boost::property_tree::ptree);
    put_default_parameters(*in);

    // SOLVING THE PROBLEM
    std::shared_ptr<boost::property_tree::ptree> out(new boost::property_tree::ptree);
    super_capacitor.run(in, out);

    // POSTPROCESSING QUANTITIES OF INTEREST
    std::vector<double> max_temperature = 
        cache::to_vector<double>(out->get<std::string>("max_temperature"));
    BOOST_FOREACH(double const & val, max_temperature) { 
        std::cout<<"  "<<val;
    }
    std::cout<<"\n";
    std::vector<std::string> capacitor_state = 
        cache::to_vector<std::string>(out->get<std::string>("capacitor_state"));
    BOOST_FOREACH(std::string const & val, capacitor_state) { 
        std::cout<<"  "<<val;
    }
    std::cout<<"\n";

    std::cout<<"goodbye cruel world\n";

    return 0;
}