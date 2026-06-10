// Root build script. Kotlin JVM plugin version is declared here and applied by
// subprojects. No Android Gradle Plugin yet (Phase A adds it for ":app").
plugins {
    kotlin("jvm") version "2.0.21" apply false
}
