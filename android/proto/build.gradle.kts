// :proto — pure Kotlin/JVM implementation of the phone2pad wire protocol.
// Kept free of Android SDK / AGP so it builds & tests with just a JDK + Gradle
// wrapper. Phase A's ":app" module will depend on this.
plugins {
    kotlin("jvm")
}

repositories {
    mavenCentral()
}

dependencies {
    testImplementation(kotlin("test"))
}

kotlin {
    jvmToolchain(21)
}

tasks.test {
    useJUnitPlatform()
    testLogging {
        events("passed", "skipped", "failed")
    }
}
