/* SWIG interface file for pyschedlib */

%module pyschedlib

%{
#include <filesystem>
#include <optional>
#include <utility>
#include <vector>
#include <string>
#include <generators/uunifast_discard_weibull.hpp>
#include <protocols/scenario.hpp>
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
%}

// Include standard SWIG typemaps for STL types available in older SWIG
// (std_optional.i and std_tuple.i are not available on SWIG < 4.1)
%include "std_vector.i"
%include "std_string.i"
// Explicitly instantiate vector<double> for convenient Python list interop
namespace std { %template(vector_double) vector<double>; }

// Minimal custom typemap for std::optional<std::pair<double,double>>
// Accepts either None or a 2-sequence of floats
%typemap(in) std::optional<std::pair<double,double>> {
  if ($input == Py_None) {
    $1 = std::nullopt;
  } else {
    PyObject *seq = PySequence_Fast($input, "expected a 2-tuple (float, float) or None");
    if (!seq) SWIG_fail;
    Py_ssize_t n = PySequence_Fast_GET_SIZE(seq);
    if (n != 2) {
      Py_DECREF(seq);
      SWIG_exception_fail(SWIG_TypeError, "expected a 2-tuple (float, float)");
    }
    PyObject *i0 = PySequence_Fast_GET_ITEM(seq, 0);
    PyObject *i1 = PySequence_Fast_GET_ITEM(seq, 1);
    double a = PyFloat_AsDouble(i0);
    if (PyErr_Occurred()) { Py_DECREF(seq); SWIG_fail; }
    double b = PyFloat_AsDouble(i1);
    if (PyErr_Occurred()) { Py_DECREF(seq); SWIG_fail; }
    $1 = std::make_optional(std::make_pair(a, b));
    Py_DECREF(seq);
  }
}

%typemap(typecheck) std::optional<std::pair<double,double>> {
  $1 = ($input == Py_None || (PySequence_Check($input) && PySequence_Size($input) == 2)) ? 1 : 0;
}

// Also support const reference variant used in some declarations
%typemap(in) const std::optional<std::pair<double,double>> & ($*1_ltype temp) {
  if ($input == Py_None) {
    temp = std::nullopt;
  } else {
    PyObject *seq = PySequence_Fast($input, "expected a 2-tuple (float, float) or None");
    if (!seq) SWIG_fail;
    if (PySequence_Fast_GET_SIZE(seq) != 2) { Py_DECREF(seq); SWIG_exception_fail(SWIG_TypeError, "expected a 2-tuple (float, float)"); }
    double a = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(seq, 0));
    if (PyErr_Occurred()) { Py_DECREF(seq); SWIG_fail; }
    double b = PyFloat_AsDouble(PySequence_Fast_GET_ITEM(seq, 1));
    if (PyErr_Occurred()) { Py_DECREF(seq); SWIG_fail; }
    temp = std::make_optional(std::make_pair(a, b));
    Py_DECREF(seq);
  }
  $1 = &temp;
}
%typemap(typecheck) const std::optional<std::pair<double,double>> & {
  $1 = ($input == Py_None || (PySequence_Check($input) && PySequence_Size($input) == 2)) ? 1 : 0;
}

// Return Python list copies for vector members to avoid dangling references
%typemap(varout) std::vector<protocols::scenario::Task> {
  PyObject *list = PyList_New($1.size());
  if (!list) SWIG_fail;
  for (std::size_t i = 0; i < $1.size(); ++i) {
    protocols::scenario::Task *tmp = new protocols::scenario::Task($1[i]);
    PyObject *py = SWIG_NewPointerObj(SWIG_as_voidptr(tmp), SWIGTYPE_p_protocols__scenario__Task, SWIG_POINTER_OWN);
    if (!py) { Py_DECREF(list); SWIG_fail; }
    PyList_SET_ITEM(list, i, py); // Steals reference
  }
  $result = list;
}

%typemap(varout) std::vector<protocols::scenario::Job> {
  PyObject *list = PyList_New($1.size());
  if (!list) SWIG_fail;
  for (std::size_t i = 0; i < $1.size(); ++i) {
    protocols::scenario::Job *tmp = new protocols::scenario::Job($1[i]);
    PyObject *py = SWIG_NewPointerObj(SWIG_as_voidptr(tmp), SWIGTYPE_p_protocols__scenario__Job, SWIG_POINTER_OWN);
    if (!py) { Py_DECREF(list); SWIG_fail; }
    PyList_SET_ITEM(list, i, py);
  }
  $result = list;
}

// Map common C++ exceptions to Python exceptions for all following declarations
%exception {
  try {
    $action
  } catch (const std::invalid_argument& e) {
    SWIG_exception(SWIG_ValueError, e.what());
  } catch (const std::out_of_range& e) {
    SWIG_exception(SWIG_IndexError, e.what());
  } catch (const std::runtime_error& e) {
    SWIG_exception(SWIG_RuntimeError, e.what());
  } catch (const std::logic_error& e) {
    SWIG_exception(SWIG_RuntimeError, e.what());
  } catch (const std::exception& e) {
    SWIG_exception(SWIG_RuntimeError, e.what());
  }
}

// Instantiate templates
namespace std {
    %template(vector_Job) vector<protocols::scenario::Job>;
    %template(vector_Task) vector<protocols::scenario::Task>;
}

// Optional typemap handled above; no explicit template needed

// Ignore any unwanted operators or methods if necessary

// Declare the classes to wrap
namespace protocols {
namespace scenario {

class Job {
public:
    double arrival;
    double duration;
};

class Task {
public:
    unsigned long long id;
    double utilization;
    double period;
    std::vector<Job> jobs;
};

class Setting {
public:
    std::vector<Task> tasks;
};

} // namespace scenario
} // namespace protocols

// Wrap generator functions (match actual namespaces and signatures)
namespace generators {
// Enable Python keyword arguments only for these APIs to avoid SWIG 511 warnings
%feature("kwargs") uunifast_discard_weibull;
%feature("kwargs") generate_tasksets;
%feature("kwargs") from_utilizations;

protocols::scenario::Setting add_taskets(
    const protocols::scenario::Setting& first,
    const protocols::scenario::Setting& second);

protocols::scenario::Setting uunifast_discard_weibull(
    std::size_t nb_tasks,
    double total_utilization,
    double umax,
    double umin,
    double success_rate,
    double compression_rate,
    const std::optional<std::pair<double, double>>& a_special_need = std::nullopt);

void generate_tasksets(
    std::string output_path,
    std::size_t nb_taskset,
    std::size_t nb_tasks,
    double total_utilization,
    double umax,
    double umin,
    double success_rate,
    double compression_rate,
    std::optional<std::pair<double, double>> a_special_need = std::nullopt,
    std::size_t nb_cores = 1);

protocols::scenario::Setting from_utilizations(
    const std::vector<double>& utilizations,
    double success_rate,
    double compression_rate);
} // namespace generators

// Custom function for JSON parsing
%inline %{
protocols::scenario::Setting from_json_setting(const std::string& json_str) {
    rapidjson::Document doc;
    if (doc.Parse(json_str.c_str()).HasParseError()) {
        throw std::runtime_error("Invalid JSON string: " + std::string(GetParseError_En(doc.GetParseError())));
    }
    if (!doc.IsObject()) {
        throw std::runtime_error("JSON must be an object");
    }
    return protocols::scenario::from_json_setting(doc);
}
// Safer helpers that return copies to avoid dangling references/lifetime issues
std::vector<protocols::scenario::Task> get_tasks_copy(const protocols::scenario::Setting& s) {
    return s.tasks; // return by value -> independent lifetime
}
std::vector<protocols::scenario::Job> get_jobs_copy(const protocols::scenario::Task& t) {
    return t.jobs; // return by value -> independent lifetime
}
// Convenience: parse directly to a standalone vector<Task>
std::vector<protocols::scenario::Task> tasks_from_json(const std::string& json_str) {
    auto s = from_json_setting(json_str);
    return s.tasks; // copy out
}

// File IO convenience wrappers (avoid exposing std::filesystem::path directly)
void write_setting_file(const std::string& file, const protocols::scenario::Setting& s) {
    protocols::scenario::write_file(file, s);
}
protocols::scenario::Setting read_setting_file(const std::string& file) {
    return protocols::scenario::read_file(file);
}
%}

%pythoncode %{
def safe_tasks_from_json(json_str):
    """Return a standalone list-like of Tasks from JSON (no lifetime pitfalls)."""
    return tasks_from_json(json_str)

# Override fragile members with safe properties returning copies
def _Setting_tasks_get(self):
    return get_tasks_copy(self)

def _Task_jobs_get(self):
    return get_jobs_copy(self)

# Replace properties on generated classes
try:
    Setting.tasks = property(_Setting_tasks_get)
    Task.jobs = property(_Task_jobs_get)
except NameError:
    # Classes not yet defined during import sequencing; ignore as SWIG places this at end
    pass
%}
