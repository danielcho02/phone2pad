// :app — the PhantomPad phone-side sensor app (Phase A).
// Black, full-screen, landscape "pad" Activity that streams raw multitouch to the
// PC over TCP :38917. Reuses the wire protocol from :proto (no re-implementation).
plugins {
    id("com.android.application")
    kotlin("android")
}

android {
    namespace = "com.phantompad"
    // Built against the installed android-36.1 platform; AGP resolves 36 to the
    // highest installed 36.x. minSdk 28 per docs/06.
    compileSdk = 36

    defaultConfig {
        applicationId = "com.phantompad"
        minSdk = 28
        targetSdk = 36
        versionCode = 1
        versionName = "0.1-phase-a"
    }

    buildTypes {
        debug {
            isMinifyEnabled = false
        }
    }

    compileOptions {
        // JDK 21 is the installed toolchain (matches :proto's jvmToolchain(21)).
        sourceCompatibility = JavaVersion.VERSION_21
        targetCompatibility = JavaVersion.VERSION_21
    }
}

kotlin {
    jvmToolchain(21)
}

dependencies {
    implementation(project(":proto"))
    implementation("androidx.core:core-ktx:1.13.1")
}
