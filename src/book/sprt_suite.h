#pragma once

#include <string>

// Generates an SPRT opening suite from an existing book.csv: positions at the book's frontier
// (no legal successor is itself a well-supported book position), filtered to a balanced score
// window. Used as a diverse set of SPRT start positions in place of a small fixed fixture.
namespace sprt_suite {

/** Read `bookFile` (book.csv format) and write frontier positions as an EPD to `outputFile`.
 *  Returns 0 on success, 1 on failure (with a message on stderr). */
int run(const std::string& bookFile, const std::string& outputFile);

}  // namespace sprt_suite
