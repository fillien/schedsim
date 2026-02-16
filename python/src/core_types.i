/* SWIG typemaps for schedsim::core types */
/* Converts Duration/TimePoint/Frequency/Power/Energy to/from Python floats */

%{
#include <schedsim/core/types.hpp>
#include <schedsim/core/power_domain.hpp>
#include <filesystem>
#include <vector>
%}

// ============================================================================
// Duration <-> Python float (seconds)
// ============================================================================

%typemap(in) schedsim::core::Duration {
    if (PyFloat_Check($input)) {
        $1 = schedsim::core::duration_from_seconds(PyFloat_AsDouble($input));
    } else if (PyLong_Check($input)) {
        $1 = schedsim::core::duration_from_seconds(static_cast<double>(PyLong_AsLong($input)));
    } else {
        SWIG_exception_fail(SWIG_TypeError, "Expected a float or int for Duration");
    }
}

%typemap(out) schedsim::core::Duration {
    $result = PyFloat_FromDouble(schedsim::core::duration_to_seconds($1));
}

%typemap(typecheck, precedence=SWIG_TYPECHECK_DOUBLE) schedsim::core::Duration {
    $1 = (PyFloat_Check($input) || PyLong_Check($input)) ? 1 : 0;
}

// Const reference variant
%typemap(in) const schedsim::core::Duration& (schedsim::core::Duration temp) {
    if (PyFloat_Check($input)) {
        temp = schedsim::core::duration_from_seconds(PyFloat_AsDouble($input));
    } else if (PyLong_Check($input)) {
        temp = schedsim::core::duration_from_seconds(static_cast<double>(PyLong_AsLong($input)));
    } else {
        SWIG_exception_fail(SWIG_TypeError, "Expected a float or int for Duration");
    }
    $1 = &temp;
}

%typemap(typecheck, precedence=SWIG_TYPECHECK_DOUBLE) const schedsim::core::Duration& {
    $1 = (PyFloat_Check($input) || PyLong_Check($input)) ? 1 : 0;
}

// ============================================================================
// TimePoint <-> Python float (seconds since epoch)
// ============================================================================

%typemap(in) schedsim::core::TimePoint {
    if (PyFloat_Check($input)) {
        $1 = schedsim::core::time_from_seconds(PyFloat_AsDouble($input));
    } else if (PyLong_Check($input)) {
        $1 = schedsim::core::time_from_seconds(static_cast<double>(PyLong_AsLong($input)));
    } else {
        SWIG_exception_fail(SWIG_TypeError, "Expected a float or int for TimePoint");
    }
}

%typemap(out) schedsim::core::TimePoint {
    $result = PyFloat_FromDouble(schedsim::core::time_to_seconds($1));
}

%typemap(typecheck, precedence=SWIG_TYPECHECK_DOUBLE) schedsim::core::TimePoint {
    $1 = (PyFloat_Check($input) || PyLong_Check($input)) ? 1 : 0;
}

// Const reference variant
%typemap(in) const schedsim::core::TimePoint& (schedsim::core::TimePoint temp) {
    if (PyFloat_Check($input)) {
        temp = schedsim::core::time_from_seconds(PyFloat_AsDouble($input));
    } else if (PyLong_Check($input)) {
        temp = schedsim::core::time_from_seconds(static_cast<double>(PyLong_AsLong($input)));
    } else {
        SWIG_exception_fail(SWIG_TypeError, "Expected a float or int for TimePoint");
    }
    $1 = &temp;
}

%typemap(typecheck, precedence=SWIG_TYPECHECK_DOUBLE) const schedsim::core::TimePoint& {
    $1 = (PyFloat_Check($input) || PyLong_Check($input)) ? 1 : 0;
}

// ============================================================================
// Frequency <-> Python float (MHz)
// ============================================================================

%typemap(in) schedsim::core::Frequency {
    if (PyFloat_Check($input)) {
        $1 = schedsim::core::Frequency{PyFloat_AsDouble($input)};
    } else if (PyLong_Check($input)) {
        $1 = schedsim::core::Frequency{static_cast<double>(PyLong_AsLong($input))};
    } else {
        SWIG_exception_fail(SWIG_TypeError, "Expected a float or int for Frequency (MHz)");
    }
}

%typemap(out) schedsim::core::Frequency {
    $result = PyFloat_FromDouble($1.mhz);
}

%typemap(typecheck, precedence=SWIG_TYPECHECK_DOUBLE) schedsim::core::Frequency {
    $1 = (PyFloat_Check($input) || PyLong_Check($input)) ? 1 : 0;
}

// ============================================================================
// Power <-> Python float (mW)
// ============================================================================

%typemap(in) schedsim::core::Power {
    if (PyFloat_Check($input)) {
        $1 = schedsim::core::Power{PyFloat_AsDouble($input)};
    } else if (PyLong_Check($input)) {
        $1 = schedsim::core::Power{static_cast<double>(PyLong_AsLong($input))};
    } else {
        SWIG_exception_fail(SWIG_TypeError, "Expected a float or int for Power (mW)");
    }
}

%typemap(out) schedsim::core::Power {
    $result = PyFloat_FromDouble($1.mw);
}

%typemap(typecheck, precedence=SWIG_TYPECHECK_DOUBLE) schedsim::core::Power {
    $1 = (PyFloat_Check($input) || PyLong_Check($input)) ? 1 : 0;
}

// ============================================================================
// Energy -> Python float (mJ) - output only
// ============================================================================

%typemap(out) schedsim::core::Energy {
    $result = PyFloat_FromDouble($1.mj);
}

// ============================================================================
// std::filesystem::path <- Python string
// ============================================================================

%typemap(in) const std::filesystem::path& (std::filesystem::path temp) {
    if (PyUnicode_Check($input)) {
        const char* str = PyUnicode_AsUTF8($input);
        if (!str) SWIG_fail;
        temp = std::filesystem::path(str);
        $1 = &temp;
    } else if (PyBytes_Check($input)) {
        const char* str = PyBytes_AsString($input);
        if (!str) SWIG_fail;
        temp = std::filesystem::path(str);
        $1 = &temp;
    } else {
        SWIG_exception_fail(SWIG_TypeError, "Expected a string path");
    }
}

%typemap(typecheck, precedence=SWIG_TYPECHECK_STRING) const std::filesystem::path& {
    $1 = (PyUnicode_Check($input) || PyBytes_Check($input)) ? 1 : 0;
}

// ============================================================================
// std::string_view <- Python string
// ============================================================================

%typemap(in) std::string_view (std::string temp) {
    if (PyUnicode_Check($input)) {
        const char* str = PyUnicode_AsUTF8($input);
        if (!str) SWIG_fail;
        temp = str;
        $1 = std::string_view(temp);
    } else if (PyBytes_Check($input)) {
        Py_ssize_t len;
        char* str;
        if (PyBytes_AsStringAndSize($input, &str, &len) < 0) SWIG_fail;
        temp = std::string(str, len);
        $1 = std::string_view(temp);
    } else {
        SWIG_exception_fail(SWIG_TypeError, "Expected a string");
    }
}

%typemap(typecheck, precedence=SWIG_TYPECHECK_STRING) std::string_view {
    $1 = (PyUnicode_Check($input) || PyBytes_Check($input)) ? 1 : 0;
}

// ============================================================================
// std::vector<schedsim::core::Processor*> <- Python list
// ============================================================================

%typemap(in) std::vector<schedsim::core::Processor*> {
    if (!PyList_Check($input)) {
        SWIG_exception_fail(SWIG_TypeError, "Expected a list of Processor objects");
    }
    Py_ssize_t size = PyList_Size($input);
    $1.reserve(size);
    for (Py_ssize_t i = 0; i < size; ++i) {
        PyObject* item = PyList_GetItem($input, i);
        void* ptr = nullptr;
        int res = SWIG_ConvertPtr(item, &ptr, $descriptor(schedsim::core::Processor*), 0);
        if (!SWIG_IsOK(res)) {
            SWIG_exception_fail(SWIG_TypeError, "List item is not a Processor");
        }
        $1.push_back(reinterpret_cast<schedsim::core::Processor*>(ptr));
    }
}

%typemap(typecheck, precedence=SWIG_TYPECHECK_POINTER) std::vector<schedsim::core::Processor*> {
    $1 = PyList_Check($input) ? 1 : 0;
}

// ============================================================================
// std::vector<schedsim::core::CStateLevel> <- Python list of tuples
// Each tuple: (level: int, scope: int, wake_latency: float, power: float)
// scope: 0 = PerProcessor, 1 = DomainWide
// ============================================================================

%typemap(in) std::vector<schedsim::core::CStateLevel> {
    if (!PyList_Check($input)) {
        SWIG_exception_fail(SWIG_TypeError, "Expected a list of (level, scope, wake_latency, power) tuples");
    }
    Py_ssize_t size = PyList_Size($input);
    $1.reserve(size);
    for (Py_ssize_t i = 0; i < size; ++i) {
        PyObject* item = PyList_GetItem($input, i);
        if (!PyTuple_Check(item) || PyTuple_Size(item) != 4) {
            SWIG_exception_fail(SWIG_TypeError, "Expected (level, scope, wake_latency, power) tuple");
        }
        int level = static_cast<int>(PyLong_AsLong(PyTuple_GetItem(item, 0)));
        if (PyErr_Occurred()) SWIG_fail;
        int scope_int = static_cast<int>(PyLong_AsLong(PyTuple_GetItem(item, 1)));
        if (PyErr_Occurred()) SWIG_fail;
        double wake_latency = PyFloat_AsDouble(PyTuple_GetItem(item, 2));
        if (PyErr_Occurred()) SWIG_fail;
        double power = PyFloat_AsDouble(PyTuple_GetItem(item, 3));
        if (PyErr_Occurred()) SWIG_fail;

        schedsim::core::CStateScope scope = (scope_int == 0)
            ? schedsim::core::CStateScope::PerProcessor
            : schedsim::core::CStateScope::DomainWide;

        $1.push_back(schedsim::core::CStateLevel{
            level, scope,
            schedsim::core::duration_from_seconds(wake_latency),
            schedsim::core::Power{power}
        });
    }
}

%typemap(typecheck, precedence=SWIG_TYPECHECK_POINTER) std::vector<schedsim::core::CStateLevel> {
    $1 = PyList_Check($input) ? 1 : 0;
}

// ============================================================================
// Output typemaps for std::vector<schedsim::core::Task*>
// ============================================================================

%typemap(out) std::vector<schedsim::core::Task*> {
    PyObject* list = PyList_New($1.size());
    if (!list) SWIG_fail;
    for (std::size_t i = 0; i < $1.size(); ++i) {
        PyObject* py = SWIG_NewPointerObj(
            SWIG_as_voidptr($1[i]),
            $descriptor(schedsim::core::Task*),
            0  // Don't own - Platform owns these
        );
        if (!py) { Py_DECREF(list); SWIG_fail; }
        PyList_SET_ITEM(list, i, py);
    }
    $result = list;
}

// ============================================================================
// std::vector<schedsim::io::JobParams> <- Python list of (arrival, duration) tuples
// ============================================================================

%typemap(in) const std::vector<schedsim::io::JobParams>& (std::vector<schedsim::io::JobParams> temp) {
    if (!PyList_Check($input)) {
        SWIG_exception_fail(SWIG_TypeError, "Expected a list of (arrival, duration) tuples");
    }
    Py_ssize_t size = PyList_Size($input);
    temp.reserve(size);
    for (Py_ssize_t i = 0; i < size; ++i) {
        PyObject* item = PyList_GetItem($input, i);
        if (!PyTuple_Check(item) || PyTuple_Size(item) != 2) {
            SWIG_exception_fail(SWIG_TypeError, "Expected (arrival, duration) tuple");
        }
        double arrival = PyFloat_AsDouble(PyTuple_GetItem(item, 0));
        if (PyErr_Occurred()) SWIG_fail;
        double duration = PyFloat_AsDouble(PyTuple_GetItem(item, 1));
        if (PyErr_Occurred()) SWIG_fail;

        temp.push_back(schedsim::io::JobParams{
            schedsim::core::time_from_seconds(arrival),
            schedsim::core::duration_from_seconds(duration)
        });
    }
    $1 = &temp;
}

%typemap(typecheck, precedence=SWIG_TYPECHECK_POINTER) const std::vector<schedsim::io::JobParams>& {
    $1 = PyList_Check($input) ? 1 : 0;
}
