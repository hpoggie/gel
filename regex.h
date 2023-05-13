#ifndef REGEX_H
#define REGEX_H

#include <string>
#include <regex>

const auto token_regex = std::regex(
  "[^\\S\\n]*(,@|[\\[\\]\\{\\}\\(\\)'`,]|\"(?:\\\\.|[^\\\\\"])*\"?|;.*|[^(\\s\\[\\]\\{\\}'\"`,;)]+|\\n)");

std::vector<std::string> match_str(std::string input) {
/*
  Black magic regex fuckery

  Escapes to
  \s*([\[\]\{\}\(\)'`]|"(?:\\.|[^\\"])*"?|;.*|[^(\s\[\]\{\}'"`,;)]+)
  
  Explanation:

  - \s*                         skip leading whitespace
  - [\[\]\{\}\(\)'`,]           Match one of []{}()'`,
  - "(?:\\.|[^\\"])*"?          Match any number of characters starting with " and ending with "
  if not preceded by \
  - ;.*                         Match anything preceded by ;
  - [^(\s\[\]\{\}'"`,;)]+       Match 1 or more of anything that isn't whitespace or []{}'"`,;
*/
  std::smatch match;

  // This is OK to do because C++ moves this under the hood rather than copying
  std::vector<std::string> ret;

  while (std::regex_search(input, match, token_regex)) {
    // This is OK to do because push_back copies
    // I *think* match[1] is OK to do because if something bad happened, regex_search would
    // return false. TODO: is this true?
    ret.push_back(match[1].str());

    input = match.suffix().str();
  }

  return ret;
}

/*
std::vector<std::string> match_lines(std::string input) {
  // Separate input into lines
  std::smatch match;
  std::vector<std::string> ret;

  while (std::regex_search(input, match, std::regex("[^\\n]+"))) {
    ret.push_back(match[0].str());
    input = match.suffix().str();
  }

  return ret;
}
*/
#endif
