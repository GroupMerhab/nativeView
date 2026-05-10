/* nv_win_env_options.cpp — Create WebView2 environment with --allow-file-access-from-files.
 * Enables direct file:// navigation which may render correctly where virtual host shows blank.
 */
#ifdef _WIN32

#include <WebView2.h>
#include <WebView2EnvironmentOptions.h>
#include <wrl.h>

using namespace Microsoft::WRL;

extern "C" {

HRESULT nv_win_create_environment_with_file_access(
    ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler* handler)
{
  ComPtr<ICoreWebView2EnvironmentOptions> opts;
  HRESULT hr = MakeAndInitialize<CoreWebView2EnvironmentOptions>(&opts);
  if (FAILED(hr) || !opts)
    return hr;
  hr = opts->put_AdditionalBrowserArguments(L"--allow-file-access-from-files");
  if (FAILED(hr))
    return hr;

  return CreateCoreWebView2EnvironmentWithOptions(
      nullptr, nullptr, opts.Get(), handler);
}

} /* extern "C" */

#endif /* _WIN32 */
