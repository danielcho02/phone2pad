// Tiny recursive-descent JSON reader for loading test vectors (test-only).
// Supports the subset used by proto/test-vectors/*.json: objects, arrays,
// strings, integers, booleans, null. Mirrors pc/.../tests/json_lite.hpp.
package com.phantompad.proto

sealed interface Json {
    data class Obj(val entries: Map<String, Json>) : Json {
        operator fun get(key: String): Json =
            entries[key] ?: error("json: missing key '$key'")
        fun contains(key: String): Boolean = entries.containsKey(key)
    }
    data class Arr(val items: List<Json>) : Json
    data class Str(val value: String) : Json
    data class Num(val value: Long) : Json
    data class Bool(val value: Boolean) : Json
    data object Null : Json

    val string: String get() = (this as Str).value
    val long: Long get() = (this as Num).value
    val int: Int get() = (this as Num).value.toInt()
    val bool: Boolean get() = (this as Bool).value
    val array: List<Json> get() = (this as Arr).items
    val obj: Obj get() = this as Obj
}

class JsonParser(private val s: String) {
    private var i = 0

    fun parse(): Json {
        skipWs()
        val v = parseValue()
        skipWs()
        return v
    }

    private fun skipWs() {
        while (i < s.length && s[i].isWhitespace()) i++
    }

    private fun peek(): Char = if (i < s.length) s[i] else ' '

    private fun parseValue(): Json {
        skipWs()
        return when (val c = peek()) {
            '{' -> parseObject()
            '[' -> parseArray()
            '"' -> Json.Str(parseString())
            't', 'f' -> parseBool()
            'n' -> parseNull()
            else -> if (c == '-' || c.isDigit()) parseNumber()
            else error("json: unexpected '$c' at $i")
        }
    }

    private fun parseObject(): Json {
        i++ // {
        val map = LinkedHashMap<String, Json>()
        skipWs()
        if (peek() == '}') { i++; return Json.Obj(map) }
        while (true) {
            skipWs()
            require(peek() == '"') { "json: expected key at $i" }
            val key = parseString()
            skipWs()
            require(peek() == ':') { "json: expected ':' at $i" }
            i++
            map[key] = parseValue()
            skipWs()
            when (peek()) {
                ',' -> { i++; continue }
                '}' -> { i++; break }
                else -> error("json: expected ',' or '}' at $i")
            }
        }
        return Json.Obj(map)
    }

    private fun parseArray(): Json {
        i++ // [
        val items = ArrayList<Json>()
        skipWs()
        if (peek() == ']') { i++; return Json.Arr(items) }
        while (true) {
            items.add(parseValue())
            skipWs()
            when (peek()) {
                ',' -> { i++; continue }
                ']' -> { i++; break }
                else -> error("json: expected ',' or ']' at $i")
            }
        }
        return Json.Arr(items)
    }

    private fun parseString(): String {
        i++ // opening quote
        val sb = StringBuilder()
        while (i < s.length) {
            val c = s[i++]
            when {
                c == '"' -> return sb.toString()
                c == '\\' -> {
                    when (val e = s[i++]) {
                        '"' -> sb.append('"')
                        '\\' -> sb.append('\\')
                        '/' -> sb.append('/')
                        'n' -> sb.append('\n')
                        't' -> sb.append('\t')
                        'r' -> sb.append('\r')
                        'b' -> sb.append('\b')
                        'f' -> sb.append('\u000C')
                        else -> error("json: unsupported escape")
                    }
                }
                else -> sb.append(c)
            }
        }
        error("json: unterminated string")
    }

    private fun parseBool(): Json {
        return when {
            s.startsWith("true", i) -> { i += 4; Json.Bool(true) }
            s.startsWith("false", i) -> { i += 5; Json.Bool(false) }
            else -> error("json: invalid literal at $i")
        }
    }

    private fun parseNull(): Json {
        require(s.startsWith("null", i)) { "json: invalid literal at $i" }
        i += 4
        return Json.Null
    }

    private fun parseNumber(): Json {
        val start = i
        if (peek() == '-') i++
        while (i < s.length && s[i].isDigit()) i++
        return Json.Num(s.substring(start, i).toLong())
    }
}

fun parseJson(text: String): Json = JsonParser(text).parse()
