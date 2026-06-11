// Phase A adds ":app" (the Android application module that depends on ":proto").
// ":proto" stays a pure Kotlin/JVM module; ":app" applies the Android Gradle Plugin.
pluginManagement {
    repositories {
        google()
        mavenCentral()
        gradlePluginPortal()
    }
}

dependencyResolutionManagement {
    repositoriesMode.set(RepositoriesMode.PREFER_SETTINGS)
    repositories {
        google()
        mavenCentral()
    }
}

rootProject.name = "phone2pad"

include(":proto")
include(":app")
