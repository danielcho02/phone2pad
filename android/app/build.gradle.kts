// :app — the phone2pad phone-side sensor app (Phase A).
// Black, full-screen, landscape "pad" Activity that streams raw multitouch to the
// PC over TCP :38917. Reuses the wire protocol from :proto (no re-implementation).
import java.util.Properties

plugins {
    id("com.android.application")
    kotlin("android")
}

// Release signing is driven by an un-committed android/keystore.properties (see
// keystore.properties.example). When the file is absent — fresh checkout, CI — the
// release build stays unsigned instead of failing, so assembleRelease still builds.
// package-release.ps1 only publishes the APK/AAB when a real keystore is present.
val keystorePropsFile = rootProject.file("keystore.properties")
val keystoreProps = Properties().apply {
    if (keystorePropsFile.exists()) {
        keystorePropsFile.inputStream().use { load(it) }
    }
}
val hasReleaseKeystore = keystorePropsFile.exists() &&
    keystoreProps.getProperty("storeFile") != null

android {
    namespace = "com.phone2pad"
    // Built against the installed android-36.1 platform; AGP resolves 36 to the
    // highest installed 36.x. minSdk 28 per docs/06.
    compileSdk = 36

    defaultConfig {
        applicationId = "com.phone2pad"
        minSdk = 28
        targetSdk = 36
        versionCode = 3
        versionName = "0.2.1"
    }

    signingConfigs {
        if (hasReleaseKeystore) {
            create("release") {
                storeFile = rootProject.file(keystoreProps.getProperty("storeFile"))
                storePassword = keystoreProps.getProperty("storePassword")
                keyAlias = keystoreProps.getProperty("keyAlias")
                keyPassword = keystoreProps.getProperty("keyPassword")
            }
        }
    }

    buildTypes {
        debug {
            isMinifyEnabled = false
        }
        release {
            // Keep Phase B behaviour intact: no code shrinking / resource stripping.
            isMinifyEnabled = false
            isShrinkResources = false
            // Signed only when a keystore is configured; otherwise unsigned.
            signingConfig = if (hasReleaseKeystore) {
                signingConfigs.getByName("release")
            } else {
                null
            }
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
