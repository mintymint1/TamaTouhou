#include "framework.h"

int main(int argc, char **argv) {
  const char *filter = argc > 1 ? argv[1] : nullptr;
  int run = 0, failedTests = 0;
  for (auto &t : registry()) {
    if (filter && !std::strstr(t.name, filter)) continue;
    int before = failures();
    t.fn();
    run++;
    bool ok = failures() == before;
    if (!ok) failedTests++;
    std::printf("%s %s\n", ok ? "  ok  " : "  FAIL", t.name);
  }
  std::printf("\n%d tests, %d failed\n", run, failedTests);
  return failedTests ? 1 : 0;
}
