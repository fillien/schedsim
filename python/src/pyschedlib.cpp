#include <filesystem>
#include <generators/uunifast_discard_weibull.hpp>
#include <protocols/scenario.hpp>
#include <pybind11/cast.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

namespace py = pybind11;

PYBIND11_MODULE(pyschedlib, m)
{
        m.doc() = "Python bindings for the schedlib C++ library";

        py::class_<protocols::scenario::Job>(m, "Job")
            .def_readonly("arrival", &protocols::scenario::Job::arrival, "Arrival time of the job")
            .def_readonly("duration", &protocols::scenario::Job::duration, "Duration of the job");

        py::class_<protocols::scenario::Task>(m, "Task")
            .def_readonly("id", &protocols::scenario::Task::id, "Task identifier")
            .def_readonly(
                "utilization", &protocols::scenario::Task::utilization, "Utilization factor")
            .def_readonly("period", &protocols::scenario::Task::period, "Period of the task")
            .def_readonly("jobs", &protocols::scenario::Task::jobs, "List of jobs in the task");

        py::class_<protocols::scenario::Setting>(m, "Setting")
            .def_readonly(
                "tasks", &protocols::scenario::Setting::tasks, "List of tasks in the task set");

        m.def("add_tasksets", &generators::add_taskets, py::arg("first"), py::arg("second"));

        m.def(
            "uunifast_discard_weibull",
            &generators::uunifast_discard_weibull,
            py::arg("nb_tasks"),
            py::arg("total_utilization"),
            py::arg("umax"),
            py::arg("success_rate"),
            py::arg("compression_rate"),
            py::kw_only(),
            py::arg("a_special_need") = std::nullopt,
            R"pbdoc(
            Generates a task set using the UUniFast-Discard method with Weibull distribution.

            This function creates a set of tasks with utilizations summing to approximately the
            specified total utilization (within a 0.01 tolerance), using the UUniFast-Discard
            algorithm. Task periods are chosen from a predefined set, and WCET is computed as
            utilization times period.

            Args:
                nb_tasks (int): Number of tasks to generate (must be >= 1).
                total_utilization (float): Total utilization to distribute (must be >= 0).
                umax (float): Maximum utilization per task (must be between 0 and 1).
                success_rate (float): Success rate for tasks (must be between 0 and 1).
                compression_rate (float): Compression rate for tasks (must be between 0 and 1).
                a_special_need (Optional[Tuple[float, float]]): Optional special requirements,
                    defaults to None.

            Returns:
                Setting: A task set containing the generated tasks.

            Raises:
                ValueError: If nb_tasks < 1, total_utilization < 0, umax, success_rate, or
                    compression_rate is not in [0, 1], or if total_utilization cannot be achieved
                    with nb_tasks and umax.
        )pbdoc");

        m.def(
            "generate_tasksets",
            &generators::generate_tasksets,
            py::arg("output_path"),
            py::arg("nb_taskset"),
            py::arg("nb_tasks"),
            py::arg("total_utilization"),
            py::arg("umax"),
            py::arg("success_rate"),
            py::arg("compression_rate"),
            py::kw_only(),
            py::arg("a_special_need") = std::nullopt,
            py::arg("nb_cores") = 1,
            R"pbdoc(
            Generates multiple task sets in parallel and writes them to files.

            This function uses uunifast_discard_weibull to generate the specified number of task
            sets, writing each to a separate file in the output directory (named "1", "2", etc.).
            Generation is parallelized across the specified number of CPU cores.

            Args:
                output_path (str): Directory to write task set files (must exist and be a directory).
                nb_taskset (int): Number of task sets to generate (must be >= 1).
                nb_tasks (int): Number of tasks per task set (must be >= 1).
                total_utilization (float): Total utilization per task set (must be >= 0).
                umax (float): Maximum utilization per task (must be between 0 and 1).
                success_rate (float): Success rate for tasks (must be between 0 and 1).
                compression_rate (float): Compression rate for tasks (must be between 0 and 1).
                a_special_need (Optional[Tuple[float, float]]): Optional special requirements,
                    defaults to None.
                nb_cores (int): Number of CPU cores for parallel generation, defaults to 1.

            Raises:
                ValueError: If output_path does not exist or is not a directory, or if other
                    parameters are invalid (see uunifast_discard_weibull).
        )pbdoc");

        m.def(
            "from_json_setting",
            [](const std::string& json_str) -> protocols::scenario::Setting {
                    rapidjson::Document doc;
                    if (doc.Parse(json_str.c_str()).HasParseError()) {
                            throw py::value_error("Invalid JSON string");
                    }
                    if (!doc.IsObject()) { throw py::value_error("JSON must be an object"); }
                    return protocols::scenario::from_json_setting(doc);
            },
            py::arg("json_str"),
            R"pbdoc(
                Creates a Setting object from a JSON string.

                Args:
                    json_str (str): A JSON string containing the serialized Setting data.

                Returns:
                    Setting: A Setting object created from the JSON data.

                Raises:
                    ValueError: If the JSON string is invalid or does not represent a valid object.
                )pbdoc");
}
