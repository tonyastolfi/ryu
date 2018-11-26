// Ryu - generator of Ninja files for Modern C++ projects.
//
#include <deque>
#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include <google/protobuf/text_format.h>

#include <ryu.pb.h>

using namespace std;
using namespace google::protobuf;

int usage(int, char **);
void generate(const RyuConfig &config, ostream *out);

template <typename T> bool read_text_proto(const string &file, T *msg) {
  ifstream ifs(file.c_str());
  if (!ifs.good()) {
    return false;
  }

  ostringstream oss;
  oss << ifs.rdbuf();

  return TextFormat::ParseFromString(oss.str(), msg);
}

string build_flags_one(string base, const RyuFlags &flags) {
  switch (flags.directive_case()) {
  case RyuFlags::kReset:
    return flags.reset();
  case RyuFlags::kAppend:
    return base + " " + flags.append();
  case RyuFlags::kPrepend:
    return flags.prepend() + " " + base;
  case RyuFlags::kReplace:
    return std::regex_replace(base, std::regex{flags.replace().pattern()},
                              flags.replace().with().c_str());
  default:
    return base;
  }
}

regex whitespace{"[ \t\n\r\v]+"};

template <typename Range>
string build_flags_all(string base, Range &&all_flags) {
  for (const auto &flag : all_flags) {
    base =
        regex_replace(build_flags_one(std::move(base), flag), whitespace, " ");
  }
  return base;
}

string package_compile_flags(const RyuPackage &package) {
  if (package.has_prefix() && package.has_include_dir() &&
      package.include_dir()[0] != '/') {
    ostringstream oss;
    oss << "-I" << package.prefix();
    if (package.prefix().back() != '/') {
      oss << "/";
    }
    oss << package.include_dir();
    return oss.str();
  }
  if (package.has_include_dir()) {
    return string("-I") + package.include_dir();
  }
  return "";
}

string package_link_flags(const RyuPackage &package) {
  if (package.has_prefix() && package.has_lib_dir() &&
      package.lib_dir()[0] != '/') {
    ostringstream oss;
    oss << "-L" << package.prefix();
    if (package.prefix().back() != '/') {
      oss << "/";
    }
    oss << package.lib_dir();
    return oss.str();
  }
  if (package.has_lib_dir()) {
    return string("-L") + package.lib_dir();
  }
  return "";
}

std::regex src_file{"^(.*)/(.*)\\.(c|cc|cpp|cxx|h|hpp|hxx|inc|ipp)$"};

const char *src_dir = "$1";
const char *src_stem = "$2";
const char *src_ext = "$3";

void generate_library(RyuPreamble preamble,
                      unordered_map<string, RyuPackage> &dependencies,
                      const string &compile_flags, const string &link_flags,
                      const RyuLibrary &library, ostream &out) {
  string lib_compile_flags =
      build_flags_all(compile_flags, library.compile_flags());

  for (const auto &dependency_name : library.dependency()) {
    const RyuPackage &dependency =
        dependencies[dependency_name]; // TODO: check for not found
  }

  string lib_link_flags = build_flags_all(link_flags, library.link_flags());

  const bool override_compile_flags = lib_compile_flags != compile_flags;
  const bool override_link_flags = lib_link_flags != link_flags;

  ostringstream objs;
  for (const auto &src : library.src()) {
    const string obj =
        library.name() + "/" + std::regex_replace(src, src_file, "$1/$2.o");

    objs << obj << " ";

    out << "build " << obj << ": cxx " << src << endl;
    if (override_compile_flags) {
      out << "  cxxflags = " << lib_compile_flags << endl;
    }
    out << endl;
  }

  out << "build "
      << "build/lib" << library.name() << ".a: link_exe " << objs.str() << endl;
  if (override_link_flags) {
    out << "  ldflags = " << lib_link_flags << endl;
  }
  out << endl;
}

void generate_config(RyuPreamble preamble, const RyuConfig &config,
                     ostream &out) {
  const string compile_flags = build_flags_all("", preamble.compile_flags());
  const string link_flags = build_flags_all("", preamble.link_flags());

  out << "cxxflags = " << compile_flags << endl << endl;

  out << "ldflags = " << link_flags << endl << endl;

  out << "rule cxx" << endl
      << "  command = " << preamble.compile_command()
      << " $cxxflags -c $in -o $out" << endl
      << endl;

  out << "rule link_exe" << endl
      << "  command = " << preamble.compile_command() << " $ldflags $in -o $out"
      << endl
      << endl;

  unordered_map<string, RyuPackage> dependencies;

  for (const auto &dependency : config.dependency()) {
    dependencies[dependency.name()] = dependency;
  }

  for (const auto &library : config.library()) {
    generate_library(preamble, dependencies, compile_flags, link_flags, library,
                     out);
  }
}

int main(int argc, char **argv) {
  if (argc < 2) {
    return usage(argc, argv);
  }
  string ryu_binary = argv[0];
  string input_file = argv[1];

  RyuPreamble preamble;
  if (!read_text_proto(ryu_binary + ".preamble", &preamble)) {
    cerr << "ERROR: reading preamble" << endl;
    return 1;
  }

  RyuConfig config;
  if (!read_text_proto(input_file, &config)) {
    cerr << "ERROR: reading input file" << endl;
    return 1;
  }

  ofstream ofs("build.ninja");
  generate_config(preamble, config, cout);
  return 0;
}

int usage(int argc, char **argv) {
  cerr << "usage: " << argv[0] << " <FILE-NAME>.ryu" << endl;
  return 1;
}
