/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
package main

import (
	/*
		#include <stdint.h>
		#include <stdbool.h>
		typedef const char cchar;
		typedef struct {
			bool autoNonVnRestore;
			bool ddFreeStyle;
			bool macroEnabled;
			bool autoCapitalizeMacro;
			bool spellCheckWithDicts;
			const char *outputCharset;
			bool modernStyle;
			bool freeMarking;
		} FcitxBambooEngineOption;
	*/
	"C"
	"bamboo-core"
	"runtime/cgo"
	"unsafe"
	"os/signal"
	"syscall"
)
import (
	"bufio"
	"os"
	"strings"
)

//export Init
func Init() {
	signal.Ignore(syscall.SIGPIPE)
}

//export EngineProcessKeyEvent
func EngineProcessKeyEvent(engine uintptr, keyVal, state uint32) bool {
	bambooEngine, ok := cgo.Handle(engine).Value().(*FcitxBambooEngine)
	if !ok {
		return false
	}
	return bambooEngine.preeditProcessKeyEvent(keyVal, state)
}

//export EngineSetRestoreKeyStroke
func EngineSetRestoreKeyStroke(engine uintptr) {
	bambooEngine, ok := cgo.Handle(engine).Value().(*FcitxBambooEngine)
	if !ok {
		return
	}
	bambooEngine.shouldRestoreKeyStrokes = true
}


//export EnginePullPreedit
func EnginePullPreedit(engine uintptr) *C.char {
	bambooEngine, ok := cgo.Handle(engine).Value().(*FcitxBambooEngine)
	if !ok {
		return nil
	}
	return C.CString(bambooEngine.preeditText)
}

//export EngineCommitPreedit
func EngineCommitPreedit(engine uintptr) {
	bambooEngine, ok := cgo.Handle(engine).Value().(*FcitxBambooEngine)
	if !ok {
		return
	}
	bambooEngine.commitPreeditAndReset(bambooEngine.getPreeditString())
}

//export EnginePullCommit
func EnginePullCommit(engine uintptr) *C.char {
	bambooEngine, ok := cgo.Handle(engine).Value().(*FcitxBambooEngine)
	if !ok {
		return nil
	}
	var commitText = bambooEngine.commitText
	bambooEngine.commitText = ""
	return C.CString(commitText)
}

//export EngineSetOption
func EngineSetOption(engine uintptr, option *C.FcitxBambooEngineOption) {
	bambooEngine, ok := cgo.Handle(engine).Value().(*FcitxBambooEngine)
	if !ok {
		return
	}
	bambooEngine.autoNonVnRestore = bool(option.autoNonVnRestore)
	bambooEngine.ddFreeStyle = bool(option.ddFreeStyle)
	bambooEngine.macroEnabled = bool(option.macroEnabled)
	bambooEngine.autoCapitalizeMacro = bool(option.autoCapitalizeMacro)
	bambooEngine.spellCheckWithDicts = bool(option.spellCheckWithDicts)
	bambooEngine.outputCharset = C.GoString(option.outputCharset)
	flags := bamboo.EstdFlags
	if option.modernStyle {
		flags &= ^bamboo.EstdToneStyle
	} else {
		flags |= bamboo.EstdToneStyle
	}

	if option.freeMarking {
		flags |= bamboo.EfreeToneMarking
	} else {
		flags &= ^bamboo.EfreeToneMarking
	}
	bambooEngine.preeditor.SetFlag(flags)
}

//export NewEngine
func NewEngine(name *C.cchar, dictHandle uintptr, tableHandle uintptr) uintptr {
	dict, ok := cgo.Handle(dictHandle).Value().(*map[string]bool)
	if !ok {
		return 0
	}

	table, ok := cgo.Handle(tableHandle).Value().(*MacroTable)
	if !ok {
		return 0
	}

	imName := C.GoString(name)

	var engine = &FcitxBambooEngine{
		preeditor:               bamboo.NewEngine(bamboo.ParseInputMethod(bamboo.InputMethodDefinitions, imName), bamboo.EstdFlags),
		macroTable:              table,
		dictionary:              *dict,
		autoNonVnRestore:        true,
		ddFreeStyle:             true,
		macroEnabled:            false,
		autoCapitalizeMacro:     false,
		lastKeyWithShift:        false,
		spellCheckWithDicts:     true,
		preeditText:             "",
		commitText:              "",
		shouldRestoreKeyStrokes: false,
		outputCharset:           "Unicode",
	}
	return uintptr(cgo.NewHandle(engine))
}

//export NewCustomEngine
func NewCustomEngine(definition **C.char, dictHandle uintptr, tableHandle uintptr) uintptr {
	dict, ok := cgo.Handle(dictHandle).Value().(*map[string]bool)
	if !ok {
		return 0
	}

	table, ok := cgo.Handle(tableHandle).Value().(*MacroTable)
	if !ok {
		return 0
	}
	var definitions = map[string]bamboo.InputMethodDefinition{
		"Custom": map[string]string{},
	}
	def := (*[1<<20 - 1]*C.char)(unsafe.Pointer(definition))

	i := 0
	for def[i] != nil {
		definitions["Custom"][C.GoString(def[i])] = C.GoString(def[i+1])
		i += 2
	}

	var engine = &FcitxBambooEngine{
		preeditor:               bamboo.NewEngine(bamboo.ParseInputMethod(definitions, "Custom"), bamboo.EstdFlags),
		macroTable:              table,
		dictionary:              *dict,
		autoNonVnRestore:        true,
		ddFreeStyle:             true,
		macroEnabled:            false,
		autoCapitalizeMacro:     false,
		lastKeyWithShift:        false,
		spellCheckWithDicts:     false,
		preeditText:             "",
		commitText:              "",
		shouldRestoreKeyStrokes: false,
		outputCharset:           "Unicode",
	}

	return uintptr(cgo.NewHandle(engine))
}

//export NewMacroTable
func NewMacroTable(definition **C.char) uintptr {
	var table = &MacroTable{
		mTable: map[string]string{},
	}
	def := (*[1<<20 - 1]*C.char)(unsafe.Pointer(definition))
	i := 0
	for def[i] != nil {
		table.mTable[C.GoString(def[i])] = C.GoString(def[i+1])
		i += 2
	}

	return uintptr(cgo.NewHandle(table))
}

//export DeleteObject
func DeleteObject(handle uintptr) {
	cgo.Handle(handle).Delete()
}

//export ResetEngine
func ResetEngine(engine uintptr) {
	bambooEngine, ok := cgo.Handle(engine).Value().(*FcitxBambooEngine)
	if !ok {
		return
	}
	bambooEngine.commitPreeditAndReset("")
}

func toCStringArray(strs []string) **C.char {

	array := C.malloc(C.size_t(len(strs)+1) * C.size_t(unsafe.Sizeof(uintptr(0))))
	// convert the C array to a Go Array so we can index it
	a := (*[1<<20 - 1]*C.char)(array)

	for idx, substring := range strs {
		a[idx] = C.CString(substring)
	}
	a[len(strs)] = nil
	return (**C.char)(array)
}

//export GetCharsetNames
func GetCharsetNames() **C.char {
	return toCStringArray(bamboo.GetCharsetNames())
}

//export GetInputMethodNames
func GetInputMethodNames() **C.char {
	names := make([]string, len(bamboo.InputMethodDefinitions))
	i := 0
	for imName := range bamboo.InputMethodDefinitions {
		names[i] = imName
		i++
	}
	return toCStringArray(names)
}

//export NewDictionary
func NewDictionary(fd uintptr) uintptr {
	var data = map[string]bool{}
	f := os.NewFile(fd, "dict")
	rd := bufio.NewReader(f)
	for {
		line, _, err := rd.ReadLine()
		if err != nil {
			break
		}
		if len(line) == 0 {
			continue
		}
		var tmp = []byte(strings.ToLower(string(line)))
		data[string(tmp)] = true
	}
	return uintptr(cgo.NewHandle(&data))
}

func main() {}
