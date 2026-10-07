#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
namespace ades {
struct Coordinate { std::int32_t x=0,y=0; };
std::vector<Coordinate> load_dimacs_co_gz(const std::string& path,std::size_t expected_vertices=0);
}
