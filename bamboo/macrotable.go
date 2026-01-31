/*
 * SPDX-FileCopyrightText: 2022-2022 CSSlayer <wengxt@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 *
 */
package main

import "strings"

type MacroTable struct {
	mTable map[string]string
}

func (e *MacroTable) HasKey(key string) bool {
	return e.mTable[strings.ToLower(key)] != ""
}
func (e *MacroTable) GetText(key string) string {
	return e.mTable[strings.ToLower(key)]
}
