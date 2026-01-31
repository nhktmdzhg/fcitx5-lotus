/*
 * SPDX-FileCopyrightText: 2018 Luong Thanh Lam <ltlam93@gmail.com>
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
package main

import (
	"bamboo-core"
	"strings"
	"unicode"
	"unicode/utf8"
)

type FcitxBambooEngine struct {
	preeditor               bamboo.IEngine
	macroTable              *MacroTable
	dictionary              map[string]bool
	autoNonVnRestore        bool
	ddFreeStyle             bool
	macroEnabled            bool
	autoCapitalizeMacro     bool
	lastKeyWithShift        bool
	spellCheckWithDicts     bool
	preeditText             string
	commitText              string
	shouldRestoreKeyStrokes bool
	outputCharset           string
}

const (
	FcitxShiftMask   = 1 << 0
	FcitxLockMask    = 1 << 1
	FcitxControlMask = 1 << 2
	FcitxMod1Mask    = 1 << 3

	/* The next few modifiers are used by XKB so we skip to the end.
	 * Bits 15 - 23 are currently unused. Bit 29 is used internally.
	 */

	FcitxForwardMask = 1 << 25
	FcitxIgnoredMask = FcitxForwardMask

	FcitxSuperMask = 1 << 26
	FcitxHyperMask = 1 << 27
	FcitxMetaMask  = 1 << 28
)
const (
	FcitxBackSpace       = 0xff08
	FcitxSpace           = 0x020
	FcitxTab             = 0xff09
)

const (
	VnCaseAllSmall uint8 = iota + 1
	VnCaseAllCapital
	VnCaseNoChange
)

func determineMacroCase(str string) uint8 {
	var chars = []rune(str)
	if unicode.IsLower(chars[0]) {
		return VnCaseAllSmall
	} else {
		for _, c := range chars[1:] {
			if unicode.IsLower(c) {
				return VnCaseNoChange
			}
		}
	}
	return VnCaseAllCapital
}

func (e *FcitxBambooEngine) expandMacro(str string) string {
	var macroText = e.macroTable.GetText(str)
	if e.autoCapitalizeMacro {
		switch determineMacroCase(str) {
		case VnCaseAllSmall:
			return strings.ToLower(macroText)
		case VnCaseAllCapital:
			return strings.ToUpper(macroText)
		}
	}
	return macroText
}

func (e *FcitxBambooEngine) getMacroText() (bool, string) {
	if !e.macroEnabled {
		return false, ""
	}
	var text = e.preeditor.GetProcessedString(bamboo.PunctuationMode)
	if e.macroTable.HasKey(text) {
		return true, e.expandMacro(text)
	}
	return false, ""
}

func (e *FcitxBambooEngine) shouldFallbackToEnglish(checkVnRune bool) bool {
	if !e.autoNonVnRestore {
		return false
	}
	var vnSeq = e.preeditor.GetProcessedString(bamboo.VietnameseMode | bamboo.LowerCase)
	var vnRunes = []rune(vnSeq)
	if len(vnRunes) == 0 {
		return false
	}
	if ok, _ := e.getMacroText(); ok {
		return false
	}
	// we want to allow dd even in non-vn sequence, because dd is used a lot in abbreviation
	if e.ddFreeStyle && !bamboo.HasAnyVietnameseVower(vnSeq) &&
		(vnRunes[len(vnRunes)-1] == 'd' || strings.ContainsRune(vnSeq, 'đ')) {
		return false
	}
	if checkVnRune && !bamboo.HasAnyVietnameseRune(vnSeq) {
		return false
	}
	return !e.preeditor.IsValid(false)
}

func (e *FcitxBambooEngine) getProcessedString(mode bamboo.Mode) string {
	return e.preeditor.GetProcessedString(mode)
}

func (e *FcitxBambooEngine) getRawKeyLen() int {
	return len(e.preeditor.GetProcessedString(bamboo.EnglishMode | bamboo.FullText))
}

func (e *FcitxBambooEngine) getPreeditString() string {
	if e.macroEnabled {
		return e.getProcessedString(bamboo.PunctuationMode)
	}
	if e.shouldFallbackToEnglish(true) {
		return e.getProcessedString(bamboo.EnglishMode)
	}
	return e.getProcessedString(bamboo.VietnameseMode)
}

func (e *FcitxBambooEngine) updateLastKeyWithShift(keyVal, state uint32) {
	if e.preeditor.CanProcessKey(rune(keyVal)) {
		e.lastKeyWithShift = state&FcitxShiftMask != 0
	} else {
		e.lastKeyWithShift = false
	}
}
func (e *FcitxBambooEngine) runeCount() int {
	return utf8.RuneCountInString(e.getPreeditString())
}

func (e *FcitxBambooEngine) getBambooInputMode() bamboo.Mode {
	if e.shouldFallbackToEnglish(false) {
		return bamboo.EnglishMode
	}
	return bamboo.VietnameseMode
}

func inKeyList(list []rune, key rune) bool {
	for _, s := range list {
		if s == key {
			return true
		}
	}
	return false
}

func (e *FcitxBambooEngine) toUpper(keyRune rune) rune {
	var keyMapping = map[rune]rune{
		'[': '{',
		']': '}',
		'{': '[',
		'}': ']',
	}

	if upperSpecialKey, found := keyMapping[keyRune]; found && inKeyList(e.preeditor.GetInputMethod().AppendingKeys, keyRune) {
		keyRune = upperSpecialKey
	}
	return keyRune
}

func (e *FcitxBambooEngine) mustFallbackToEnglish() bool {
	if !e.autoNonVnRestore {
		return false
	}
	var vnSeq = e.getProcessedString(bamboo.VietnameseMode | bamboo.LowerCase)
	var vnRunes = []rune(vnSeq)
	if len(vnRunes) == 0 {
		return false
	}
	// we want to allow dd even in non-vn sequence, because dd is used a lot in abbreviation
	if e.ddFreeStyle && strings.ContainsRune(vnSeq, 'đ') {
		return false
	}
	if e.spellCheckWithDicts {
		return !e.dictionary[vnSeq]
	}
	return !e.preeditor.IsValid(true)
}

func (e *FcitxBambooEngine) getCommitText(keyVal, state uint32) (string, bool) {
	var keyRune = rune(keyVal)
	oldText := e.getPreeditString()
	// restore key strokes by pressing Shift + Space
	if e.shouldRestoreKeyStrokes {
		e.shouldRestoreKeyStrokes = false
		e.preeditor.RestoreLastWord(!bamboo.HasAnyVietnameseRune(oldText))
		return e.getPreeditString(), false
	}
	if e.preeditor.CanProcessKey(keyRune) {
		if state&FcitxLockMask != 0 {
			keyRune = e.toUpper(keyRune)
		}
		e.preeditor.ProcessKey(keyRune, e.getBambooInputMode())
		if inKeyList(e.preeditor.GetInputMethod().AppendingKeys, keyRune) {
			var newText string
			if e.shouldFallbackToEnglish(true) {
				newText = e.getProcessedString(bamboo.EnglishMode)
			} else {
				newText = e.getProcessedString(bamboo.VietnameseMode)
			}
			if fullSeq := e.preeditor.GetProcessedString(bamboo.VietnameseMode); len(fullSeq) > 0 && rune(fullSeq[len(fullSeq)-1]) == keyRune {
				// [[ => [
				var ret = e.getPreeditString()
				var lastRune = rune(ret[len(ret)-1])
				var isWordBreakRune = bamboo.IsWordBreakSymbol(lastRune)
				// TODO: THIS IS HACKING
				if isWordBreakRune {
					e.preeditor.RemoveLastChar(false)
					e.preeditor.ProcessKey(' ', bamboo.EnglishMode)
				}
				return ret, isWordBreakRune
			} else if l := []rune(newText); len(l) > 0 && keyRune == l[len(l)-1] {
				// f] => f]
				var isWordBreakRune = bamboo.IsWordBreakSymbol(keyRune)
				if isWordBreakRune {
					e.preeditor.RemoveLastChar(false)
					e.preeditor.ProcessKey(' ', bamboo.EnglishMode)
				}
				return oldText + string(keyRune), isWordBreakRune
			} else {
				// ] => o?
				return e.getPreeditString(), false
			}
		} else if e.macroEnabled {
			return e.getProcessedString(bamboo.PunctuationMode), false
		} else {
			return e.getPreeditString(), false
		}
	} else if bamboo.IsWordBreakSymbol(keyRune) {
		// macro processing
		if e.macroEnabled {
			var keyS = string(keyRune)
			if keyVal == FcitxSpace && e.macroTable.HasKey(oldText) {
				e.preeditor.Reset()
				return e.expandMacro(oldText) + keyS, keyVal == FcitxSpace
			} else {
				e.preeditor.ProcessKey(keyRune, e.getBambooInputMode())
				return oldText + keyS, keyVal == FcitxSpace
			}
		}
		if bamboo.HasAnyVietnameseRune(oldText) && e.mustFallbackToEnglish() {
			e.preeditor.RestoreLastWord(false)
			newText := e.preeditor.GetProcessedString(bamboo.EnglishMode) + string(keyRune)
			e.preeditor.ProcessKey(keyRune, bamboo.EnglishMode)
			return newText, true
		}
		e.preeditor.ProcessKey(keyRune, bamboo.EnglishMode)
		return oldText + string(keyRune), true
	}
	return "", true
}

func (e *FcitxBambooEngine) encodeText(text string) string {
	return bamboo.Encode(e.outputCharset, text)
}

func (e *FcitxBambooEngine) commitPreeditAndReset(s string) {
	e.commitText = s
	e.preeditText = ""
	e.preeditor.Reset()
}

func (e *FcitxBambooEngine) updatePreedit(processedStr string) {
	var encodedStr = e.encodeText(processedStr)
	var preeditLen = uint32(len([]rune(encodedStr)))
	if preeditLen == 0 {
		e.preeditText = ""
		e.commitText = ""
		return
	}

	e.preeditText = encodedStr
}

func (e *FcitxBambooEngine) canProcessKey(keyVal uint32) bool {
	var keyRune = rune(keyVal)
	if keyVal == FcitxSpace || keyVal == FcitxBackSpace || bamboo.IsWordBreakSymbol(keyRune) {
		return true
	}
	if ok, _ := e.getMacroText(); ok && keyVal == FcitxTab {
		return true
	}
	return e.preeditor.CanProcessKey(keyRune)
}
func (e *FcitxBambooEngine) isValidState(state uint32) bool {
	if state&FcitxControlMask != 0 ||
		state&FcitxMod1Mask != 0 ||
		state&FcitxIgnoredMask != 0 ||
		state&FcitxSuperMask != 0 ||
		state&FcitxHyperMask != 0 ||
		state&FcitxMetaMask != 0 {
		return false
	}
	return true
}

func (e *FcitxBambooEngine) getComposedString(oldText string) string {
	if bamboo.HasAnyVietnameseRune(oldText) && e.mustFallbackToEnglish() {
		return e.getProcessedString(bamboo.EnglishMode)
	}
	return oldText
}

func (e *FcitxBambooEngine) preeditProcessKeyEvent(keyVal uint32, state uint32) bool {
	var rawKeyLen = e.getRawKeyLen()
	var keyRune = rune(keyVal)
	var oldText = e.getPreeditString()
	defer e.updateLastKeyWithShift(keyVal, state)

	// workaround for chrome's address bar and Google SpreadSheets
	if !e.shouldRestoreKeyStrokes {
		if !e.isValidState(state) || !e.canProcessKey(keyVal) ||
			(!e.macroEnabled && rawKeyLen == 0 && !e.preeditor.CanProcessKey(keyRune)) {
			if rawKeyLen > 0 {
				e.commitPreeditAndReset(e.getPreeditString())
			}
			return false
		}
	}

	if keyVal == FcitxBackSpace {
		if e.runeCount() == 1 {
			e.commitPreeditAndReset("")
			return true
		}
		if rawKeyLen > 0 {
			e.preeditor.RemoveLastChar(true)
			e.updatePreedit(e.getPreeditString())
			return true
		} else {
			return false
		}
	}
	if keyVal == FcitxTab {
		if ok, macText := e.getMacroText(); ok {
			e.commitPreeditAndReset(macText)
		} else {
			e.commitPreeditAndReset(e.getComposedString(oldText))
			return false
		}
		return true
	}

	newText, isWordBreakRune := e.getCommitText(keyVal, state)
	if isWordBreakRune {
		e.commitPreeditAndReset(newText)
		return true
	}
	e.updatePreedit(newText)
	return true
}
