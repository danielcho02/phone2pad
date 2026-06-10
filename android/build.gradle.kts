// Root build script. Plugin versions are declared here (apply false) and applied
// by subprojects:
//   :proto -> kotlin("jvm")            (pure JVM, no Android SDK)
//   :app   -> com.android.application + kotlin("android")
plugins {
    kotlin("jvm") version "2.0.21" apply false
    id("com.android.application") version "8.13.0" apply false
    kotlin("android") version "2.0.21" apply false
}
