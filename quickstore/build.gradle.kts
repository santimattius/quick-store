import com.android.build.api.variant.KotlinMultiplatformAndroidComponentsExtension
import org.jetbrains.kotlin.gradle.plugin.mpp.KotlinNativeTarget

plugins {
    alias(libs.plugins.androidKmpLibrary)
    alias(libs.plugins.kotlinMultiplatform)
}

kotlin {
    android {
        namespace = "com.quickstore"
        compileSdk = 35
        minSdk = 24
    }

    iosArm64()
    iosSimulatorArm64()
    iosX64()

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
    }
}
