#pragma once

#include <vector>

#include "parser.hpp"
#include "schema.hpp"

namespace preproc {

void assign_ids(Schema* schema, std::vector<FileAnalysis>* analyses);
void rewrite_sources(std::vector<FileAnalysis>* analyses);

}  // namespace preproc
