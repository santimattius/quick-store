import SwiftUI

struct ContentView: View {
    @StateObject private var runner = BenchmarkRunner()

    var body: some View {
        VStack(spacing: 24) {
            Text("QuickStore Benchmark")
                .font(.title)
                .bold()
                .padding(.top, 48)

            Button(action: {
                runner.runBenchmark()
            }) {
                Text("Run Benchmark")
                    .frame(minWidth: 160)
                    .padding()
                    .background(runner.isRunning ? Color.gray : Color.blue)
                    .foregroundColor(.white)
                    .cornerRadius(10)
            }
            .disabled(runner.isRunning)

            if runner.isRunning {
                ProgressView("Running…")
                    .padding()
            }

            if !runner.quickStoreResult.isEmpty {
                GroupBox(label: Text("QuickStore").bold()) {
                    Text(runner.quickStoreResult)
                        .font(.system(.body, design: .monospaced))
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(4)
                }

                GroupBox(label: Text("UserDefaults").bold()) {
                    Text(runner.userDefaultsResult)
                        .font(.system(.body, design: .monospaced))
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(4)
                }

                GroupBox(label: Text("Speedup").bold()) {
                    Text(runner.speedupResult)
                        .font(.system(.title3, design: .monospaced))
                        .bold()
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(4)
                }
            }

            Spacer()
        }
        .padding(.horizontal, 24)
    }
}

#Preview {
    ContentView()
}
