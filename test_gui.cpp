#include "third_party/webview.h"
int main() {
  webview::webview w(true, nullptr);
  w.set_title("Test");
  w.set_size(480, 320, WEBVIEW_HINT_NONE);
  w.set_html("<html><body><h1>Works</h1></body></html>");
  // Don't w.navigate or w.run, just compile test
  return 0;
}
