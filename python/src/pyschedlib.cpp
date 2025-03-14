#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <protocols/scenario.hpp>
#include <generators/uunifast_discard_weibull.hpp>

namespace py = pybind11;

// Define the Python module
PYBIND11_MODULE(pyschedlib, m) {
    m.doc() = "Python bindings for the schedlib C++ library";

    py::class_<protocols::scenario::Job>(m, "Job")
        .def_readonly("arrival", &protocols::scenario::Job::arrival, "Arrival time of the job")
        .def_readonly("duration", &protocols::scenario::Job::duration, "Duration of the job");

    py::class_<protocols::scenario::Task>(m, "Task")
        .def_readonly("id", &protocols::scenario::Task::id, "Task identifier")
        .def_readonly("utilization", &protocols::scenario::Task::utilization, "Utilization factor")
        .def_readonly("period", &protocols::scenario::Task::period, "Period of the task")
        .def_readonly("jobs", &protocols::scenario::Task::jobs, "List of jobs in the task");

    py::class_<protocols::scenario::Setting>(m, "Setting")
        .def_readonly("tasks", &protocols::scenario::Setting::tasks, "List of tasks in the task set");

    m.def("uunifast_discard_weibull", &generators::uunifast_discard_weibull,
          py::arg("nb_tasks"), py::arg("total_utilization"), py::arg("umax"),
          py::arg("success_rate"), py::arg("compression_rate"),
          R"pbdoc(
            Generates a task set using the UUniFast-Discard method with Weibull distribution.

            Args:
                nb_tasks (int): Number of tasks to generate.
                total_utilization (float): Total utilization to distribute.
                umax (float): Maximum utilization per task.
                success_rate (float): Success rate for job completion.
                compression_rate (float): Compression factor for Weibull distribution.

            Returns:
                Setting: A task set containing the generated tasks.

            Raises:
                RuntimeError: If total_utilization <= 0 or other invalid inputs.
          )pbdoc");
}
