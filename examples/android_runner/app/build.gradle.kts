plugins {
  id("com.android.application")
  id("org.jetbrains.kotlin.android")
}

android {
  namespace = "com.nativeview.androidrunner"
  compileSdk = 34

  defaultConfig {
    applicationId = "com.nativeview.androidrunner"
    minSdk = 24
    targetSdk = 34

    testInstrumentationRunner = "androidx.test.runner.AndroidJUnitRunner"

    externalNativeBuild {
      cmake {
        arguments += listOf(
          "-DNV_BUILD_UI=OFF",
          "-DNV_BUILD_TESTS=OFF"
        )
        targets += listOf("nv-platform-android")
      }
    }
  }

  buildTypes {
    release {
      isMinifyEnabled = false
    }
    debug {
      isMinifyEnabled = false
    }
  }

  packaging {
    jniLibs {
      useLegacyPackaging = true
    }
  }

  compileOptions {
    sourceCompatibility = JavaVersion.VERSION_17
    targetCompatibility = JavaVersion.VERSION_17
  }

  kotlinOptions {
    jvmTarget = "17"
  }

  externalNativeBuild {
    cmake {
      path = file("../../../CMakeLists.txt")
      version = "3.22.1"
    }
  }
}

dependencies {
  implementation("androidx.core:core-ktx:1.13.1")

  androidTestImplementation("androidx.test:core:1.6.1")
  androidTestImplementation("androidx.test:runner:1.6.2")
  androidTestImplementation("androidx.test.ext:junit:1.2.1")
}
