import SwiftUI
import AppKit

@main
struct TenBoxApp: App {
    @NSApplicationDelegateAdaptor(AppDelegate.self) var appDelegate

    init() {
        NSApplication.shared.setActivationPolicy(.regular)
        NSApplication.shared.activate(ignoringOtherApps: true)
        NSApplication.shared.applicationIconImage = Self.makeAppIcon()
    }

    private static func makeAppIcon() -> NSImage {
        let size = NSSize(width: 256, height: 256)
        let image = NSImage(size: size, flipped: false) { rect in
            let ctx = NSGraphicsContext.current!.cgContext

            let cornerRadius: CGFloat = 48
            let bgPath = CGPath(roundedRect: rect.insetBy(dx: 8, dy: 8),
                                cornerWidth: cornerRadius, cornerHeight: cornerRadius,
                                transform: nil)
            ctx.addPath(bgPath)
            ctx.clip()
            let colors = [
                CGColor(red: 0.18, green: 0.45, blue: 0.95, alpha: 1),
                CGColor(red: 0.10, green: 0.28, blue: 0.72, alpha: 1)
            ] as CFArray
            if let gradient = CGGradient(colorsSpace: CGColorSpaceCreateDeviceRGB(),
                                         colors: colors, locations: [0, 1]) {
                ctx.drawLinearGradient(gradient,
                                       start: CGPoint(x: rect.midX, y: rect.maxY),
                                       end: CGPoint(x: rect.midX, y: rect.minY),
                                       options: [])
            }

            let attrs: [NSAttributedString.Key: Any] = [
                .font: NSFont.systemFont(ofSize: 110, weight: .bold),
                .foregroundColor: NSColor.white,
            ]
            let text = NSAttributedString(string: "TB", attributes: attrs)
            let textSize = text.size()
            let textOrigin = CGPoint(x: (rect.width - textSize.width) / 2,
                                     y: (rect.height - textSize.height) / 2)
            text.draw(at: textOrigin)
            return true
        }
        return image
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(appDelegate.appState)
                .frame(minWidth: 800, minHeight: 500)
        }
        .commands {
            CommandGroup(replacing: .newItem) {
                Button("New VM...") {
                    appDelegate.appState.showCreateVmDialog = true
                }
                .keyboardShortcut("n")
            }
        }
    }
}

class AppState: ObservableObject {
    @Published var vms: [VmInfo] = []
    @Published var selectedVmId: String?
    @Published var showCreateVmDialog = false
    @Published var showEditVmDialog = false

    private var bridge = TenBoxBridgeWrapper()
    private var activeSessions: [String: VmSession] = [:]
    private var stateObserver: NSObjectProtocol?

    init() {
        refreshVmList()
        stateObserver = NotificationCenter.default.addObserver(
            forName: NSNotification.Name("TenBoxVmStateChanged"),
            object: nil, queue: .main
        ) { [weak self] note in
            guard let self = self else { return }
            self.refreshVmList()
            if let vmId = note.object as? String {
                let newState = self.vms.first(where: { $0.id == vmId })?.state ?? .stopped
                if newState == .stopped || newState == .crashed {
                    self.removeSession(for: vmId)
                }
            }
        }
    }

    deinit {
        if let obs = stateObserver {
            NotificationCenter.default.removeObserver(obs)
        }
    }

    func getOrCreateSession(for vmId: String) -> VmSession {
        if let existing = activeSessions[vmId] {
            return existing
        }
        let session = VmSession(vmId: vmId)
        activeSessions[vmId] = session
        return session
    }

    func removeSession(for vmId: String) {
        if let session = activeSessions[vmId] {
            session.disconnect()
        }
        activeSessions.removeValue(forKey: vmId)
    }

    func refreshVmList() {
        vms = bridge.getVmList()
    }

    func createVm(config: VmCreateConfig) {
        bridge.createVm(config: config)
        refreshVmList()
    }

    func editVm(id: String, name: String, memoryMb: Int, cpuCount: Int, netEnabled: Bool) {
        bridge.editVm(id: id, name: name, memoryMb: memoryMb, cpuCount: cpuCount, netEnabled: netEnabled)
        refreshVmList()
    }

    func deleteVm(id: String) {
        removeSession(for: id)
        bridge.deleteVm(id: id)
        refreshVmList()
    }

    func startVm(id: String) {
        bridge.startVm(id: id)
        refreshVmList()
        if let session = activeSessions[id] {
            session.connectIfNeeded()
        }
    }

    func stopVm(id: String) {
        if let session = activeSessions[id] {
            if session.ipcClient.isConnected {
                session.ipcClient.sendControl("stop")
            }
        }
        removeSession(for: id)
        bridge.stopVm(id: id)
        refreshVmList()
    }

    func rebootVm(id: String) {
        if let session = activeSessions[id], session.ipcClient.isConnected {
            session.ipcClient.sendControl("reboot")
        } else {
            bridge.rebootVm(id: id)
        }
    }

    func shutdownVm(id: String) {
        if let session = activeSessions[id], session.ipcClient.isConnected {
            session.ipcClient.sendControl("shutdown")
        } else {
            bridge.shutdownVm(id: id)
        }
    }
}

class AppDelegate: NSObject, NSApplicationDelegate {
    let appState = AppState()
    private let bridge = TenBoxBridgeWrapper()

    func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
        return false
    }

    func applicationWillTerminate(_ notification: Notification) {
        bridge.stopAllVms()
    }
}
