#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <pybind11/functional.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <simgrid/config.h>
#include <xbt/log.h>
#include <xbt/string.hpp>

#include <simgrid/s4u/Actor.hpp>
#include <simgrid/s4u/Engine.hpp>
#include <simgrid/s4u/Host.hpp>

#include <boost/intrusive_ptr.hpp>

namespace py = pybind11;
using simgrid::s4u::Actor;
using simgrid::s4u::ActorPtr;
using simgrid::s4u::Engine;
using simgrid::s4u::Host;

XBT_LOG_NEW_DEFAULT_CATEGORY(python, "python");

PYBIND11_DECLARE_HOLDER_TYPE(T, boost::intrusive_ptr<T>);

namespace {

static std::string get_simgrid_version()
{
  int major, minor, patch;
  sg_version_get(&major, &minor, &patch);
  return simgrid::xbt::string_printf("%i.%i.%i", major, minor, patch);
}

static std::string simgrid_version = get_simgrid_version();

} // namespace

PYBIND11_MODULE(simgrid, m)
{

  m.doc() = "SimGrid userspace API";

  m.attr("simgrid_version") = simgrid_version;

  m.def("info", [](char* s) { XBT_INFO("%s", s); }, "Display a logging message of default priority.");

  /* this_actor namespace */
  m.def("execute", py::overload_cast<double>(&simgrid::s4u::this_actor::execute),
        "Block the actor, computing the given amount of flops");
  m.def("execute", py::overload_cast<double, double>(&simgrid::s4u::this_actor::execute),
        "Block the actor, computing the given amount of flops at the given priority");
  m.def("yield_", &simgrid::s4u::this_actor::yield, "Yield the actor");

  /* Class Engine */
  py::class_<Engine>(m, "Engine")
      .def(py::init([](std::vector<std::string> args) -> simgrid::s4u::Engine* {
        static char noarg[] = {'\0'};
        int argc            = args.size();
        std::unique_ptr<char* []> argv(new char*[argc + 1]);
        for (int i = 0; i != argc; ++i)
          argv[i] = args[i].empty() ? noarg : &args[i].front();
        argv[argc] = nullptr;
        // Currently this can be dangling, we should wrap this somehow.
        return new simgrid::s4u::Engine(&argc, argv.get());
      }))
      .def("load_platform", &Engine::load_platform, "Load a platform file describing the environment")
      .def("load_deployment", &Engine::load_deployment, "Load a deployment file and launch the actors that it contains")
      .def("run", &Engine::run, "Run the simulation")
      .def("register_function", [](Engine*, std::string name, std::function<void(std::vector<std::string>)> f) {
        simgrid::simix::register_function(name,
            [f](std::vector<std::string> args) -> simgrid::simix::ActorCode {
          return [args, f]() { f(args); };
        });
      }, "Registers the main function of an actor that will be launched from the deployment file");

  // Currently, Host lead to segfault:
  py::class_<simgrid::s4u::Host, std::unique_ptr<Host, py::nodelete>>(m, "Host").def(
      "by_name", &Host::by_name, "Retrieve a host from its name, or die");

  py::class_<simgrid::s4u::Actor, ActorPtr>(m, "Actor", "An actor is an independent stream of execution in your distributed application");

  // Select the right template instantiation
  simgrid::s4u::ActorPtr (*create_actor)(std::string, Host*, std::function<void()>) = &Actor::create;

  m.def("create_actor", create_actor, "Create an actor");
  m.def("create_actor", [](std::string name, Host* host) -> std::function<ActorPtr(std::function<void()>)> {
    return [name, host](std::function<void()> f) -> ActorPtr {
      return simgrid::s4u::Actor::create(name, host, std::move(f));
    };
  }, "Create an actor");
}