#include <cap/version.h>
#include <pycap/property_tree_wrappers.h>
#include <pycap/energy_storage_device_wrappers.h>
#include <boost/python.hpp>
#include <string>
#include <memory>
#include <map>


BOOST_PYTHON_MODULE(_pycap)
{
    boost::python::scope().attr("__version__"        ) = cap::version()        ;
    boost::python::scope().attr("__git_branch__"     ) = cap::git_branch()     ;
    boost::python::scope().attr("__git_commit_hash__") = cap::git_commit_hash();

    boost::python::docstring_options doc_options;
    doc_options.enable_user_defined();
    doc_options.enable_py_signatures();
    doc_options.disable_cpp_signatures();

    boost::python::class_<
        pycap::EnergyStorageDeviceWrap,
        std::shared_ptr<pycap::EnergyStorageDeviceWrap>,
        boost::noncopyable >(
        "EnergyStorageDevice",
        "Wrappers for Cap.EnergyStorageDevice\n"
        "\n"
        "Example\n"
        "-------\n"
        ">>> ptree = PropertyTree()\n"
        ">>> ptree.parse_info('device.info')\n"
        ">>> device = EnergyStorageDevice(ptree)\n"
        ">>> delta_t, U = 1.0, 2.1 # units of seconds and volts\n"
        ">>> device.evolve_one_time_step_constant_voltage(delta_t,U)\n"
        ">>> I = device.get_current() # amperes \n"
        ,
        boost::python::no_init )
        .def("__init__",
boost::python::make_constructor(&pycap::build_energy_storage_device,
boost::python::default_call_policies(), boost::python::args("ptree")))
        .def("get_voltage", (&pycap::get_voltage), boost::python::args("self") )
        .def("get_current", (&pycap::get_current), boost::python::args("self") )
        .def("evolve_one_time_step_constant_current", boost::python::pure_virtual(&cap::EnergyStorageDevice::evolve_one_time_step_constant_current), boost::python::args("self", "time_step", "current") )
        .def("evolve_one_time_step_constant_voltage", boost::python::pure_virtual(&cap::EnergyStorageDevice::evolve_one_time_step_constant_voltage), boost::python::args("self", "time_step", "voltage") )
        .def("evolve_one_time_step_constant_power"  , boost::python::pure_virtual(&cap::EnergyStorageDevice::evolve_one_time_step_constant_power  ), boost::python::args("self", "time_step", "load"   ) )
        .def("evolve_one_time_step_constant_load"   , boost::python::pure_virtual(&cap::EnergyStorageDevice::evolve_one_time_step_constant_load   ), boost::python::args("self", "time_step", "power"  ) )
        .def("evolve_one_time_step_changing_current", boost::python::pure_virtual(&cap::EnergyStorageDevice::evolve_one_time_step_changing_current), boost::python::args("self", "time_step", "current") )
        .def("evolve_one_time_step_changing_voltage", boost::python::pure_virtual(&cap::EnergyStorageDevice::evolve_one_time_step_changing_voltage), boost::python::args("self", "time_step", "voltage") )
        .def("evolve_one_time_step_changing_power"  , boost::python::pure_virtual(&cap::EnergyStorageDevice::evolve_one_time_step_changing_power  ), boost::python::args("self", "time_step", "power"  ) )
        .def("evolve_one_time_step_changing_load"   , boost::python::pure_virtual(&cap::EnergyStorageDevice::evolve_one_time_step_changing_load   ), boost::python::args("self", "time_step", "load"   ) )
        .def("compute_equivalent_circuit", &pycap::compute_equivalent_circuit, "Return the PropertyTree to build an equivalent circuit model", boost::python::args("ptree") )
        .staticmethod("compute_equivalent_circuit")
//        .def_pickle(pycap::serializable_class_pickle_support<cap::EnergyStorageDevice>())
        ;

    boost::python::class_<
        boost::property_tree::ptree,
        std::shared_ptr<boost::property_tree::ptree> >(
        "PropertyTree",
        "Wrappers for Boost.PropertyTree\n"
        "\n"
        "Examples\n"
        "--------\n"
        ">>> ptree = PropertyTree()\n"
        ">>> ptree.put_double('pi', 3.14)\n"
        ">>> ptree.get_double('pi')\n"
        "3.14\n"
        ">>> ptree.get_double_with_default('sqrt2', 1,41)\n"
        "1.41\n"
        "\n"
        "Exceptions\n"
        "----------\n"
        "RuntimeError: No such node (<path>)\n"
        "    Error indicating that specified <path> does not exist.\n"
        "RuntimeError: conversion of data to type \"<type>\" failed\n"
        "    Error indicating that translation from or to <type> has\n"
        "    failed.\n"
        )
        .def("get_double"                   , &pycap::get_double                   , "Get the double at the given path."                                , boost::python::args("self", "path") )
        .def("get_string"                   , &pycap::get_string                   , "Get the string at the given path."                                , boost::python::args("self", "path") )
        .def("get_int"                      , &pycap::get_int                      , "Get the integer at the given path."                               , boost::python::args("self", "path") )
        .def("get_bool"                     , &pycap::get_bool                     , "Get the boolean at the given path."                               , boost::python::args("self", "path") )
        .def("get_double_with_default_value", &pycap::get_double_with_default_value, "Get the double at the given path or return default_value."        , boost::python::args("self", "path", "default_value") )
        .def("get_string_with_default_value", &pycap::get_string_with_default_value, "Get the string at the given path or return default_value."        , boost::python::args("self", "path", "default_value") )
        .def("get_int_with_default_value"   , &pycap::get_int_with_default_value   , "Get the integer at the given path or return default_value."       , boost::python::args("self", "path", "default_value") )
        .def("get_bool_with_default_value"  , &pycap::get_bool_with_default_value  , "Get the boolean at the given path or return default_value."       , boost::python::args("self", "path", "default_value") )
        .def("put_double"                   , &pycap::put_double                   , "Set the node at the given path to the supplied value."            , boost::python::args("self", "path", "value") )
        .def("put_string"                   , &pycap::put_string                   , "Set the node at the given path to the supplied value."            , boost::python::args("self", "path", "value") )
        .def("put_int"                      , &pycap::put_int                      , "Set the node at the given path to the supplied value."            , boost::python::args("self", "path", "value") )
        .def("put_bool"                     , &pycap::put_bool                     , "Set the node at the given path to the supplied value."            , boost::python::args("self", "path", "value") )
        .def("get_array_double"             , &pycap::get_array_double             , "Get comma separated array of double."                             , boost::python::args("self", "path") )
        .def("get_array_string"             , &pycap::get_array_string             , "Get comma separated array of string."                             , boost::python::args("self", "path") )
        .def("get_array_int"                , &pycap::get_array_int                , "Get comma separated array of integer."                            , boost::python::args("self", "path") )
        .def("get_array_bool"               , &pycap::get_array_bool               , "Get comma separated array of boolean."                            , boost::python::args("self", "path") )
        .def("parse_xml"                    , &pycap::parse_xml                    , "Read the input file at XML format and populate the PropertyTree." , boost::python::args("self", "filename") )
        .def("parse_json"                   , &pycap::parse_json                   , "Read the input file at JSON format and populate the PropertyTree.", boost::python::args("self", "filename") )
        .def("parse_info"                   , &pycap::parse_info                   , "Read the input file at INFO format and populate the PropertyTree.", boost::python::args("self", "filename") )
        .def("get_child"                    , &pycap::get_child                    , "Get the child at the given path, or throw ptree_bad_path."        , boost::python::args("self", "path") )
        .def_pickle(pycap::serializable_class_pickle_support<boost::property_tree::ptree>())
        ;
}
