#include <cstdlib>
#include <string>

namespace bcc {
char *unescape_string(const char *text) {
  std::string input(text);
  std::string output;
  for (size_t i = 0; i < input.size(); ++i) {
    if (input[i] == '\\' && i + 1 < input.size()) {
      switch (input[++i]) {
      case 'n':
        output += '\n';
        break;
      case 't':
        output += '\t';
        break;
      case '\\':
        output += '\\';
        break;
      case '"':
        output += '"';
        break;
      default:
        output += input[i];
        break;
      }
    } else {
      output += input[i];
    }
  }
  return strdup(output.c_str());
}
} // namespace bcc
