Pod::Spec.new do |s|
  s.name                = 'NativeViewIOS'
  s.version             = '1.0.0'
  s.summary             = 'nativeView iOS binding'
  s.ios.deployment_target = '14.0'
  s.swift_version       = '5.7'
  s.source_files        = 'Sources/NativeViewIOS/**/*.swift'
  s.resource_bundles    = {
    'NativeViewIOS' => [
      'Sources/NativeViewIOS/Resources/*',
    ],
  }
  s.vendored_frameworks = 'NativeView.xcframework'
end
