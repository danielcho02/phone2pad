// Locates and loads the shared proto/test-vectors/*.json regardless of the
// process working directory by walking up from it to find the repo's proto dir.
package com.phone2pad.proto

import java.io.File

object TestVectorLoader {

    /** Directory holding the shared test vectors (repo-root/proto/test-vectors). */
    val vectorDir: File by lazy { locateVectorDir() }

    fun load(): List<Json.Obj> =
        vectorDir.listFiles { f -> f.extension == "json" }
            ?.sortedBy { it.name }
            ?.map { parseJson(it.readText()).obj }
            ?: error("no test vectors found in $vectorDir")

    private fun locateVectorDir(): File {
        var dir: File? = File(System.getProperty("user.dir")).absoluteFile
        while (dir != null) {
            val candidate = File(dir, "proto/test-vectors")
            if (candidate.isDirectory) return candidate
            dir = dir.parentFile
        }
        error("could not locate proto/test-vectors above ${System.getProperty("user.dir")}")
    }
}

fun hexToBytes(hex: String): ByteArray {
    require(hex.length % 2 == 0) { "odd-length hex string" }
    return ByteArray(hex.length / 2) { idx ->
        val hi = Character.digit(hex[idx * 2], 16)
        val lo = Character.digit(hex[idx * 2 + 1], 16)
        require(hi >= 0 && lo >= 0) { "invalid hex digit" }
        ((hi shl 4) or lo).toByte()
    }
}

fun bytesToHex(bytes: ByteArray): String {
    val sb = StringBuilder(bytes.size * 2)
    for (b in bytes) {
        val v = b.toInt() and 0xFF
        sb.append("0123456789abcdef"[v ushr 4])
        sb.append("0123456789abcdef"[v and 0x0F])
    }
    return sb.toString()
}
