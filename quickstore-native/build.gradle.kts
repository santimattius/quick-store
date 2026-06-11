
plugins {
    alias(libs.plugins.androidLibrary)
    alias(libs.plugins.mavenPublish)
}

android {
    namespace = "com.quickstore.native_lib"
    compileSdk = libs.versions.compileSdk.get().toInt()

    defaultConfig {
        minSdk = libs.versions.minSdk.get().toInt()
        externalNativeBuild {
            cmake {
                cppFlags("-std=c++17", "-fno-exceptions", "-fno-rtti")
                arguments("-DANDROID=TRUE")
            }
        }
        ndk {
            abiFilters += listOf("arm64-v8a", "x86_64")
        }
    }

    externalNativeBuild {
        cmake {
            path = file("../quickstore/src/androidMain/cpp/CMakeLists.txt")
            version = "3.22.1"
        }
    }

    compileOptions {
        sourceCompatibility = JavaVersion.VERSION_11
        targetCompatibility = JavaVersion.VERSION_11
    }
}

mavenPublishing {
    publishToMavenCentral()
    signAllPublications()
    coordinates(
        groupId = "io.github.santimattius",
        artifactId = "quickstore-native",
        version = providers.gradleProperty("version").get()
    )
    pom {
        name.set("QuickStore Native")
        description.set("JNI bridge for QuickStore — Android native library (transitive dependency of quickstore)")
        url.set("https://github.com/santimattius/quick-store")
        licenses {
            license {
                name.set("The Apache Software License, Version 2.0")
                url.set("https://www.apache.org/licenses/LICENSE-2.0.txt")
            }
        }
        developers {
            developer {
                id.set("santimattius")
                name.set("Santiago Mattiauda")
                url.set("https://github.com/santimattius")
            }
        }
        scm {
            url.set("https://github.com/santimattius/quick-store")
            connection.set("scm:git:git://github.com/santimattius/quick-store.git")
            developerConnection.set("scm:git:ssh://git@github.com/santimattius/quick-store.git")
        }
    }
}
