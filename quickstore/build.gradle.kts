import com.android.build.api.variant.KotlinMultiplatformAndroidComponentsExtension
import org.jetbrains.kotlin.gradle.plugin.mpp.KotlinNativeTarget
import org.jetbrains.kotlin.gradle.plugin.mpp.apple.XCFramework

plugins {
    alias(libs.plugins.androidKmpLibrary)
    alias(libs.plugins.kotlinMultiplatform)
    alias(libs.plugins.mavenPublish)
}

kotlin {
    androidLibrary {
        namespace = "com.quickstore"
        compileSdk = 35
        minSdk = 24
    }

    // XCFramework output: quickstore/build/XCFrameworks/release/QuickStore.xcframework
    val xcf = XCFramework("QuickStore")
    listOf(iosArm64(), iosSimulatorArm64(), iosX64()).forEach {
        it.binaries.framework {
            baseName = "QuickStore"
            isStatic = true
            xcf.add(this)
        }
    }

    targets.withType<KotlinNativeTarget>().configureEach {
        val targetName = this.name
        compilations["main"].cinterops {
            create("quickstore") {
                defFile(project.file("src/nativeInterop/cinterop/quickstore.def"))
                packageName("quickstore.cinterop")
                includeDirs(rootProject.file("core/include"))
                extraOpts("-libraryPath", rootProject.projectDir.resolve("build/ios/$targetName/core").absolutePath)
            }
        }
    }

    sourceSets {
        commonMain.dependencies {}
        androidMain.dependencies {
            implementation(project(":quickstore-native"))
        }
        iosMain.dependencies {}
        commonTest.dependencies {
            implementation(libs.kotlin.test)
        }
        findByName("androidHostTest")?.dependencies {
            implementation(libs.kotlin.test)
        }
    }
}

// Fat-AAR: embed libquickstore_jni.so from :quickstore-native directly into :quickstore's AAR.
//
// The com.android.kotlin.multiplatform.library plugin does not pull native libs from project
// dependencies into the androidMain variant AAR automatically (unlike the debug variant).
// We use addGeneratedSourceDirectory with a custom task that exposes a DirectoryProperty,
// so Gradle can track the output and AGP can bundle the .so files into the AAR.
abstract class SyncNativeLibsTask : DefaultTask() {
    @get:InputFiles
    @get:PathSensitive(PathSensitivity.RELATIVE)
    abstract val inputLibs: ConfigurableFileCollection

    @get:OutputDirectory
    abstract val outputDirectory: DirectoryProperty

    @TaskAction
    fun sync() {
        val outDir = outputDirectory.get().asFile
        outDir.deleteRecursively()
        outDir.mkdirs()
        project.copy {
            from(inputLibs) { include("**/*.so") }
            into(outDir)
        }
    }
}

val nativeProject = project(":quickstore-native")
val androidComponents = extensions.getByType<KotlinMultiplatformAndroidComponentsExtension>()

androidComponents.onVariants { variant ->
    // The androidMain KMP variant resolves :quickstore-native's *release* build.
    // The debug variant (used by assembleDebug) resolves :quickstore-native's debug build.
    val nativeVariantCapitalized = if (variant.name == "debug") "Debug" else "Release"

    val syncJni = tasks.register("syncNativeLibs_${variant.name}", SyncNativeLibsTask::class) {
        dependsOn(":quickstore-native:merge${nativeVariantCapitalized}NativeLibs")
        inputLibs.from(
            nativeProject.layout.buildDirectory.dir(
                "intermediates/merged_native_libs/${nativeVariantCapitalized.lowercase()}/" +
                "merge${nativeVariantCapitalized}NativeLibs/out/lib"
            )
        )
        outputDirectory.set(
            layout.buildDirectory.dir("generated/jniLibs/${variant.name}")
        )
    }

    // Register the output directory as a jniLibs source so AGP bundles it into the AAR
    @Suppress("UnstableApiUsage")
    variant.sources.jniLibs?.addGeneratedSourceDirectory(syncJni, SyncNativeLibsTask::outputDirectory)
}

// iOS cmake exec tasks — one per slice
listOf(
    Triple("iosArm64",          "arm64",  "iphoneos"),
    Triple("iosSimulatorArm64", "arm64",  "iphonesimulator"),
    Triple("iosX64",            "x86_64", "iphonesimulator")
).forEach { (targetName, arch, sdk) ->
    val buildDir = rootProject.projectDir.resolve("build/ios/$targetName")
    val cmakeBuild = tasks.register("buildIosCore_$targetName", Exec::class) {
        group = "build"
        description = "Builds quickstore_core static library for $targetName"
        doFirst { buildDir.mkdirs() }
        commandLine(
            "sh", "-c",
            """
            /opt/homebrew/bin/cmake -S ${rootProject.projectDir} -B $buildDir \
              -DCMAKE_SYSTEM_NAME=iOS \
              -DCMAKE_OSX_ARCHITECTURES=$arch \
              -DCMAKE_OSX_SYSROOT=$sdk \
              -DQUICKSTORE_BUILD_TESTS=OFF \
              -DCMAKE_BUILD_TYPE=Release && \
            /opt/homebrew/bin/cmake --build $buildDir --target quickstore_core
            """.trimIndent()
        )
    }
    afterEvaluate {
        tasks.matching { t ->
            t.name.contains("cinterop", ignoreCase = true) &&
            t.name.contains(targetName, ignoreCase = true)
        }.configureEach {
            dependsOn(cmakeBuild)
        }
        // Also wire framework link tasks (linkRelease/DebugFrameworkIos<Target>) so
        // the static lib is available when Kotlin/Native links the framework slices.
        tasks.matching { t ->
            (t.name.contains("linkRelease", ignoreCase = true) ||
             t.name.contains("linkDebug", ignoreCase = true)) &&
            t.name.contains("Framework", ignoreCase = true) &&
            t.name.contains(targetName, ignoreCase = true)
        }.configureEach {
            dependsOn(cmakeBuild)
        }
    }
}

mavenPublishing {
    publishToMavenCentral()
    signAllPublications()
    coordinates(
        groupId = "io.github.santimattius",
        artifactId = "quickstore",
        version = providers.gradleProperty("version").get()
    )
    pom {
        name.set("QuickStore")
        description.set("A fast, MMKV-compatible key-value store for Kotlin Multiplatform (Android + iOS)")
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
