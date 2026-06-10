// Phase 0: only the pure-Kotlin/JVM protocol module exists. Phase A adds ":app"
// (the Android application module that depends on ":proto").
rootProject.name = "phantompad"

include(":proto")
