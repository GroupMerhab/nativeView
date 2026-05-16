// SPDX-License-Identifier: Apache-2.0

import Foundation
import UIKit

/// Sandbox file access under the app `Documents` directory (`fs.*`).
public enum FileOps {
    public static let namespace = "fs"

    private struct ReadFileArgs: Decodable {
        let path: String
        var encoding: String?
    }

    private struct WriteFileArgs: Decodable {
        let path: String
        var encoding: String?
        let data: String
    }

    private struct PathArgs: Decodable {
        let path: String
    }

    private struct ListDirArgs: Decodable {
        var path: String?
    }

    public static func register(app: NativeViewApp) {
        app.handle("fs.readFile", permissions: []) { _, argsJson, resolve, reject in
            do {
                let a = try NVJsonWire.decode(ReadFileArgs.self, from: argsJson)
                guard !a.path.isEmpty else {
                    reject("ERR_INVALID_ARG", "path required")
                    return
                }
                let encoding = a.encoding ?? "text"
                guard let url = resolveDocumentsFileURL(path: a.path, reject: reject) else { return }
                let fileData = try Data(contentsOf: url)
                let obj: [String: Any]
                if encoding.lowercased() == "base64" {
                    obj = ["data": fileData.base64EncodedString()]
                } else {
                    obj = ["data": String(data: fileData, encoding: .utf8) ?? ""]
                }
                guard let out = NVJsonWire.jsonString(obj) else {
                    reject("ERR_JSON", "encode failed")
                    return
                }
                resolve(out)
            } catch {
                if error is NVJsonWire.WireError {
                    reject(NVRejectCode.invalidArgs, "malformed arguments")
                } else {
                    reject("ERR_IO", error.localizedDescription)
                }
            }
        }

        app.handle("fs.writeFile", permissions: []) { _, argsJson, resolve, reject in
            do {
                let a = try NVJsonWire.decode(WriteFileArgs.self, from: argsJson)
                guard !a.path.isEmpty else {
                    reject("ERR_INVALID_ARG", "path required")
                    return
                }
                let encoding = a.encoding ?? "text"
                let bytes: Data
                if encoding.lowercased() == "base64" {
                    guard let d = Data(base64Encoded: a.data, options: .ignoreUnknownCharacters) else {
                        reject("ERR_INVALID_ARG", "invalid base64")
                        return
                    }
                    bytes = d
                } else {
                    bytes = Data(a.data.utf8)
                }
                guard let url = resolveDocumentsFileURL(path: a.path, reject: reject, createParents: true) else { return }
                try bytes.write(to: url, options: .atomic)
                guard let out = NVJsonWire.jsonString([:]) else {
                    reject("ERR_JSON", "encode failed")
                    return
                }
                resolve(out)
            } catch {
                if error is NVJsonWire.WireError {
                    reject(NVRejectCode.invalidArgs, "malformed arguments")
                } else {
                    reject("ERR_IO", error.localizedDescription)
                }
            }
        }

        app.handle("fs.deleteFile", permissions: []) { _, argsJson, resolve, reject in
            do {
                let a = try NVJsonWire.decode(PathArgs.self, from: argsJson)
                guard !a.path.isEmpty else {
                    reject("ERR_INVALID_ARG", "path required")
                    return
                }
                guard let url = resolveDocumentsFileURL(path: a.path, reject: reject) else { return }
                if FileManager.default.fileExists(atPath: url.path) {
                    try FileManager.default.removeItem(at: url)
                }
                guard let out = NVJsonWire.jsonString([:]) else {
                    reject("ERR_JSON", "encode failed")
                    return
                }
                resolve(out)
            } catch {
                if error is NVJsonWire.WireError {
                    reject(NVRejectCode.invalidArgs, "malformed arguments")
                } else {
                    reject("ERR_IO", error.localizedDescription)
                }
            }
        }

        app.handle("fs.exists", permissions: []) { _, argsJson, resolve, reject in
            do {
                let a = try NVJsonWire.decode(PathArgs.self, from: argsJson)
                guard !a.path.isEmpty else {
                    reject("ERR_INVALID_ARG", "path required")
                    return
                }
                guard let url = resolveDocumentsFileURL(path: a.path, reject: reject) else { return }
                let exists = FileManager.default.fileExists(atPath: url.path)
                let obj: [String: Any] = ["exists": exists]
                guard let out = NVJsonWire.jsonString(obj) else {
                    reject("ERR_JSON", "encode failed")
                    return
                }
                resolve(out)
            } catch {
                reject(NVRejectCode.invalidArgs, "malformed arguments")
            }
        }

        app.handle("fs.listDir", permissions: []) { _, argsJson, resolve, reject in
            do {
                let a = try NVJsonWire.decode(ListDirArgs.self, from: argsJson)
                let path = a.path ?? ""
                guard let dirURL = resolveDocumentsDirectoryURL(path: path, reject: reject) else { return }
                var isDir: ObjCBool = false
                guard FileManager.default.fileExists(atPath: dirURL.path, isDirectory: &isDir), isDir.boolValue else {
                    reject("ERR_IO", "not a directory")
                    return
                }
                let names = try FileManager.default.contentsOfDirectory(atPath: dirURL.path)
                let obj: [String: Any] = ["files": names.sorted()]
                guard let out = NVJsonWire.jsonString(obj) else {
                    reject("ERR_JSON", "encode failed")
                    return
                }
                resolve(out)
            } catch {
                if error is NVJsonWire.WireError {
                    reject(NVRejectCode.invalidArgs, "malformed arguments")
                } else {
                    reject("ERR_IO", error.localizedDescription)
                }
            }
        }
    }

    private static func documentsBase() -> URL {
        FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0]
    }

    /// Percent-decodes iteratively, rejects `.` / `..` segments and `..` substrings, then resolves under Documents.
    private static func sanitizeRelativePath(_ path: String, reject: RejectCallback) -> String? {
        var working = path.trimmingCharacters(in: CharacterSet(charactersIn: "/"))
        for _ in 0..<8 {
            let before = working
            if let decoded = working.removingPercentEncoding {
                working = decoded
            }
            working = working.trimmingCharacters(in: CharacterSet(charactersIn: "/"))
            if working == before { break }
        }
        if working.unicodeScalars.contains(where: { $0.value == 0 }) {
            reject(NVRejectCode.invalidArgs, "invalid path")
            return nil
        }
        // Normalize `/`, drop empty and `.` segments; reject only literal `..` segments (not e.g. `foo..bar`).
        var parts: [String] = []
        for seg in working.split(separator: "/", omittingEmptySubsequences: false) {
            let s = String(seg)
            if s.isEmpty || s == "." { continue }
            if s == ".." {
                reject(NVRejectCode.invalidArgs, "invalid path")
                return nil
            }
            parts.append(s)
        }
        return parts.joined(separator: "/")
    }

    private static func resolveDocumentsDirectoryURL(path: String, reject: RejectCallback) -> URL? {
        guard let trimmed = sanitizeRelativePath(path, reject: reject) else {
            return nil
        }
        if trimmed.isEmpty {
            return documentsBase()
        }
        let url = documentsBase().appendingPathComponent(trimmed, isDirectory: true)
        let base = documentsBase().standardizedFileURL.path
        let resolved = url.standardizedFileURL.path
        guard resolved.hasPrefix(base + "/") || resolved == base else {
            reject(NVRejectCode.invalidArgs, "path outside Documents")
            return nil
        }
        return url
    }

    private static func resolveDocumentsFileURL(path: String, reject: RejectCallback, createParents: Bool = false) -> URL? {
        guard let trimmed = sanitizeRelativePath(path, reject: reject) else {
            return nil
        }
        guard !trimmed.isEmpty, !trimmed.hasSuffix("/") else {
            reject("ERR_INVALID_ARG", "path required")
            return nil
        }
        let url = documentsBase().appendingPathComponent(trimmed, isDirectory: false)
        let base = documentsBase().standardizedFileURL.path
        let resolved = url.standardizedFileURL.path
        guard resolved.hasPrefix(base + "/") else {
            reject(NVRejectCode.invalidArgs, "path outside Documents")
            return nil
        }
        if createParents {
            let parent = url.deletingLastPathComponent()
            try? FileManager.default.createDirectory(at: parent, withIntermediateDirectories: true)
        }
        return url
    }
}
