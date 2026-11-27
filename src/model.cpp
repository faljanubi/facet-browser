#include "stdafx.h"
#include "model.h"

namespace fb {

	const char kValueSep = '\x01';     // between values of a multi-value tag
	static const char kInfoSep = '\x1f';   // between fields of the statistics script
	static const char kPartSep = '\x02';   // between columns of a multi-column key

	// %<name>% means "one entry per value" (as in Facets); map it to $meta_sep with a
	// separator that is split again after formatting.
	static pfc::string8 expandFields(const char* pattern) {
		pfc::string8 out;
		const char* p = pattern;
		while (*p) {
			if (p[0] == '%' && p[1] == '<') {
				const char* end = strstr(p + 2, ">%");
				if (end) {
					// note: pfc strings print a char with << as a number, so append the separator as a byte
					out << "$meta_sep(" << pfc::string8(p + 2, end - (p + 2)) << ",";
					out.add_byte(kValueSep);
					out << ")";
					p = end + 2;
					continue;
				}
			}
			out.add_byte(*p++);
		}
		return out;
	}

	std::vector<pfc::string8> splitPipe(const char* pattern) {
		std::vector<pfc::string8> out;
		pfc::string8 cur;
		int depth = 0;
		bool quoted = false;
		for (const char* p = pattern; *p; ++p) {
			const char c = *p;
			if (c == '\'') quoted = !quoted;   // '' (an escaped quote) toggles twice
			else if (!quoted) {
				if (c == '(') depth++;
				else if (c == ')' && depth > 0) depth--;
				else if (c == '|' && depth == 0) { out.push_back(cur); cur.reset(); continue; }
			}
			cur.add_byte(c);
		}
		out.push_back(cur);
		return out;
	}

	pfc::string8 expandMultiValue(const char* pattern) {
		// "A|B": every track is listed under the value of A and under the value(s) of B
		pfc::string8 out;
		bool first = true;
		for (auto& piece : splitPipe(pattern)) {
			if (!first) out.add_byte(kValueSep);
			first = false;
			out << expandFields(piece);
		}
		return out;
	}

	bool patternHasMarkers(const char* pattern) {
		for (const char* p = pattern; *p; ++p) {
			if (p[0] == '%' && p[1] == '<') {
				const char* end = strstr(p + 2, ">%");
				if (end) { p = end + 1; continue; }
			}
			if (*p == '<' || *p == '>') return true;
		}
		return false;
	}

	pfc::string8 stripMarkers(const char* s) {
		pfc::string8 out;
		for (const char* c = s; *c; ++c) if (*c != '<' && *c != '>') out.add_byte(*c);
		return out;
	}

	static std::vector<pfc::string8> splitValues(const char* s) {
		std::vector<pfc::string8> out;
		const char* start = s;
		for (const char* c = s;; ++c) {
			if (*c == kValueSep || *c == 0) {
				if (c > start) {
					pfc::string8 v(start, c - start);
					if (std::find(out.begin(), out.end(), v) == out.end()) out.push_back(v);
				}
				if (*c == 0) break;
				start = c + 1;
			}
		}
		return out;
	}

	// A track's location relative to the Media Library folder when possible (what the Folder
	// columns count from), file name included.
	static pfc::string8 relativeLocation(library_manager::ptr lib, const metadb_handle_ptr& h) {
		pfc::string8 rel;
		bool ok = false;
		try { ok = lib->get_relative_path(h, rel); } catch (...) {}
		if (!ok) {
			// not in the library (playlist source): use the full path without the drive
			rel = h->get_path();
			if (rel.has_prefix("file://")) rel = rel.get_ptr() + 7;
			const char* colon = strstr(rel.c_str(), ":\\");
			if (colon) rel = colon + 2;
		}
		return rel;
	}

	// The depth-th folder (1-based) of a folder path: false when the track sits above that level.
	// deeper: there are folders below it. Empty segments ("\\\\") are skipped.
	static bool folderAt(const char* s, uint32_t depth, const char*& outStart, size_t& outLen, bool& deeper) {
		const char* p = s;
		auto next = [&](const char*& st, size_t& len) {
			while (*p == '\\' || *p == '/') ++p;
			if (!*p) return false;
			st = p;
			while (*p && *p != '\\' && *p != '/') ++p;
			len = (size_t)(p - st);
			return true;
		};
		const char* st = nullptr; size_t len = 0;
		for (uint32_t k = 1; k <= depth; ++k) if (!next(st, len)) return false;
		outStart = st; outLen = len;
		const char* st2; size_t len2;
		deeper = next(st2, len2);
		return true;
	}

	pfc::string8 folderPrefix(const metadb_handle_ptr& h, uint32_t depth) {
		pfc::string8 full = h->get_path();
		if (!full.has_prefix("file://") || depth == 0) return "";
		full = full.get_ptr() + 7;
		// the part of the path the Folder columns count from: after the Media Library folder, else after the drive
		pfc::string8 rel;
		bool ok = false;
		try { ok = library_manager::get()->get_relative_path(h, rel); } catch (...) {}
		size_t start;
		if (ok && rel.length() <= full.length() && pfc::stringEqualsI_utf8(full.get_ptr() + full.length() - rel.length(), rel)) start = full.length() - rel.length();
		else {
			const char* colon = strstr(full.c_str(), ":\\");
			if (!colon) return "";
			start = (colon + 2) - full.c_str();
		}
		// skip empty segments the same way folderParts does, then cut after the depth-th folder
		const char* s = full.c_str();
		size_t i = start;
		uint32_t found = 0;
		while (s[i]) {
			while (s[i] == '\\' || s[i] == '/') ++i;
			if (!s[i]) break;
			while (s[i] && s[i] != '\\' && s[i] != '/') ++i;
			if (!s[i]) return "";   // reached the file name: the track sits above this level
			if (++found == depth) return pfc::string8(s, i + 1);   // keep the trailing separator
		}
		return "";
	}

	// FILETIME (UTC) as "YYYY-MM-DD HH:MM:SS" local time, the format of %last_modified%.
	static pfc::string8 formatFileTime(uint64_t ft) {
		if (ft == 0 || ft == filetimestamp_invalid) return "";
		FILETIME f; f.dwLowDateTime = (DWORD)ft; f.dwHighDateTime = (DWORD)(ft >> 32);
		SYSTEMTIME utc, local;
		if (!FileTimeToSystemTime(&f, &utc) || !SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local)) return "";
		char buf[32];
		sprintf_s(buf, "%04u-%02u-%02u %02u:%02u:%02u", local.wYear, local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond);
		return buf;
	}

	// The local file a path lives in: the file itself, or for a track inside an archive
	// ("unpack://rar|<length>|file://...rar|inner path") the archive file. Empty if not local.
	static pfc::string8 containerFile(const char* path) {
		for (int depth = 0; depth < 4; ++depth) {
			if (pfc::strcmp_partial(path, "file://") == 0) return path;
			const char* sep = strstr(path, "://");
			const char* bar = sep ? strchr(sep + 3, '|') : nullptr;
			if (!bar) return "";
			char* end = nullptr;
			unsigned long len = strtoul(bar + 1, &end, 10);
			if (!end || *end != '|' || strlen(end + 1) < len) return "";
			static thread_local pfc::string8 inner;
			inner.set_string(end + 1, len);
			path = inner.c_str();
		}
		return "";
	}

	// Creation time read from disk, for tracks foobar2000 has no cached value for.
	static uint64_t readCreated(const char* path) {
		pfc::string8 file = containerFile(path);
		if (file.is_empty()) return 0;
		std::wstring w = pfc::stringcvt::string_wide_from_utf8(file.c_str() + 7).get_ptr();
		if (w.size() >= MAX_PATH && w.compare(0, 4, L"\\\\?\\") != 0)
			w = (w.compare(0, 2, L"\\\\") == 0) ? L"\\\\?\\UNC\\" + w.substr(2) : L"\\\\?\\" + w;
		WIN32_FILE_ATTRIBUTE_DATA fa;
		if (!GetFileAttributesExW(w.c_str(), GetFileExInfoStandard, &fa)) return 0;
		return ((uint64_t)fa.ftCreationTime.dwHighDateTime << 32) | fa.ftCreationTime.dwLowDateTime;
	}

	uint32_t FacetModel::intern(PaneValues& p, const std::vector<pfc::string8>& parts) {
		std::string key;
		for (size_t i = 0; i < parts.size(); ++i) { if (i) key.push_back(kPartSep); key.append(parts[i].c_str(), parts[i].length()); }
		auto it = p.lookup.find(key);
		if (it != p.lookup.end()) return it->second;
		return internNew(p, key, parts);
	}

	// A value not seen before in this pane (key: its parts joined with kPartSep).
	uint32_t FacetModel::internNew(PaneValues& p, const std::string& key, const std::vector<pfc::string8>& parts) {
		uint32_t id = (uint32_t)p.names.size();
		p.names.push_back(pfc::string8(key.c_str(), key.length()));
		p.parts.push_back(parts);
		std::vector<std::wstring> sk;
		for (size_t i = 0; i < parts.size(); ++i) {
			const bool strip = i < p.markers.size() && p.markers[i];
			sk.push_back(std::wstring(pfc::stringcvt::string_wide_from_utf8(strip ? stripMarkers(parts[i]) : parts[i]).get_ptr()));
		}
		p.sortKeys.push_back(std::move(sk));
		p.lookup.emplace(key, id);
		return id;
	}

	// A title formatting result as a number: leading/trailing spaces allowed, anything else makes it no number.
	static double parseNumber(const char* s) {
		while (*s == ' ') ++s;
		if (!*s) return NAN;
		char* end = nullptr;
		const double v = strtod(s, &end);
		if (end == s) return NAN;
		while (*end == ' ') ++end;
		return *end ? NAN : v;
	}

	static pfc::string8 formatNumber(double v) {
		char buf[64];
		if (std::fabs(v) < 1e15 && std::fabs(v - std::llround(v)) < 1e-9) sprintf_s(buf, "%lld", std::llround(v));
		else {
			sprintf_s(buf, "%.2f", v);
			size_t len = strlen(buf);
			while (len > 0 && buf[len - 1] == '0') buf[--len] = 0;
			if (len > 0 && buf[len - 1] == '.') buf[--len] = 0;
		}
		return buf;
	}

	// One script per non-folder column of the panes marked in `which` (all when empty).
	bool FacetModel::compileColumns(const std::vector<PaneSpec>& panes, const std::vector<char>& which, std::vector<ColumnJob>& cols, bool& anyFolder, pfc::string8& error) {
		auto compiler = titleformat_compiler::get();
		anyFolder = false;
		for (size_t p = 0; p < panes.size(); ++p) {
			if (!which.empty() && !which[p]) continue;
			for (size_t c = 0; c < panes[p].columns.size(); ++c) {
				const auto& cs = panes[p].columns[c];
				if (cs.folder) { anyFolder = true; continue; }
				ColumnJob col{ p, c };
				if (!compiler->compile(col.script, expandMultiValue(cs.pattern))) {
					error = PFC_string_formatter() << "The pattern of column \"" << cs.name << "\" has an error: " << cs.pattern;
					return false;
				}
				cols.push_back(col);
			}
		}
		return true;
	}

	// Folder locations of every track, kept so panes can be rebuilt without asking the library again.
	void FacetModel::ensureLocations() {
		if (m_locationsReady) return;
		auto lib = library_manager::get();
		const size_t n = m_tracks.get_count();
		m_folderOf.assign(n, 0);
		m_folders.clear();
		std::unordered_map<std::string, uint32_t> lookup;
		for (size_t t = 0; t < n; ++t) {
			const pfc::string8 rel = relativeLocation(lib, m_tracks[t]);
			// the folder part: everything before the file name
			size_t cut = 0;
			for (size_t i = 0; i < rel.length(); ++i) if (rel[i] == '\\' || rel[i] == '/') cut = i;
			std::string dir(rel.c_str(), cut);
			auto it = lookup.find(dir);
			if (it == lookup.end()) {
				it = lookup.emplace(dir, (uint32_t)m_folders.size()).first;
				m_folders.push_back(pfc::string8(dir.c_str(), dir.length()));
			}
			m_folderOf[t] = it->second;
		}
		m_locationsReady = true;
	}

	// Builds the values of the given panes, each on its own thread (they share nothing but read-only
	// input). raw[k] holds column job k's formatted text per track.
	void FacetModel::buildPanes(const std::vector<PaneSpec>& panes, const std::vector<size_t>& which, const std::vector<ColumnJob>& cols, const std::vector<std::vector<pfc::string8>>& raw) {
		auto one = [&](size_t p) {
			std::vector<const std::vector<pfc::string8>*> byColumn(panes[p].columns.size(), nullptr);
			for (size_t k = 0; k < cols.size(); ++k) if (cols[k].pane == p) byColumn[cols[k].column] = &raw[k];
			buildPane(p, panes[p], byColumn);
		};
		if (which.empty()) return;
		std::vector<std::thread> threads;
		for (size_t k = 1; k < which.size(); ++k) threads.emplace_back(one, which[k]);
		one(which[0]);
		for (auto& t : threads) t.join();
	}

	void FacetModel::buildPane(size_t p, const PaneSpec& spec, const std::vector<const std::vector<pfc::string8>*>& byColumn) {
		auto& pv = m_panes[p];
		pv = PaneValues();
		const size_t n = m_tracks.get_count();
		const size_t nc = spec.columns.size();
		pv.offsets.resize(n + 1);
		pv.deeper.assign(n, 0);
		for (auto& cs : spec.columns) pv.markers.push_back(cs.markers ? 1 : 0);
		std::vector<const char*> one(nc);
		std::vector<size_t> oneLen(nc);
		std::string key;
		std::vector<pfc::string8> parts(nc);
		for (size_t t = 0; t < n; ++t) {
			pv.offsets[t] = (uint32_t)pv.ids.size();
			bool missing = false, multi = false;
			for (size_t c = 0; c < nc && !missing; ++c) {
				const auto& cs = spec.columns[c];
				if (cs.folder) {
					bool deeper = false;
					if (!folderAt(m_folders[m_folderOf[t]], cs.depth, one[c], oneLen[c], deeper)) missing = true;   // track sits above this folder level
					else if (deeper) pv.deeper[t] = 1;
				} else {
					const pfc::string8& r = (*byColumn[c])[t];
					if (strchr(r.c_str(), kValueSep)) multi = true;
					one[c] = r.c_str(); oneLen[c] = r.length();
				}
			}
			if (missing) continue;
			if (!multi) {
				// the common case: one value per column, so exactly one item
				key.clear();
				for (size_t c = 0; c < nc; ++c) { if (c) key.push_back(kPartSep); key.append(one[c], oneLen[c]); }
				auto it = pv.lookup.find(key);
				uint32_t id;
				if (it != pv.lookup.end()) id = it->second;
				else {
					for (size_t c = 0; c < nc; ++c) parts[c].set_string(one[c], oneLen[c]);
					id = internNew(pv, key, parts);
				}
				pv.ids.push_back(id);
				continue;
			}
			// multi-value columns: the cartesian product of the values (capped to keep pathological tags in check)
			std::vector<std::vector<pfc::string8>> perCol(nc);
			for (size_t c = 0; c < nc; ++c) {
				if (spec.columns[c].folder) perCol[c].emplace_back(one[c], oneLen[c]);
				else {
					perCol[c] = splitValues((*byColumn[c])[t]);
					if (perCol[c].empty()) perCol[c].push_back("");
				}
			}
			std::vector<std::vector<pfc::string8>> combos(1);
			for (auto& vals : perCol) {
				std::vector<std::vector<pfc::string8>> next;
				for (auto& base : combos) for (auto& v : vals) {
					if (next.size() >= 64) break;
					auto x = base; x.push_back(v); next.push_back(std::move(x));
				}
				combos.swap(next);
			}
			for (auto& combo : combos) {
				uint32_t id = intern(pv, combo);
				bool dup = false;
				for (size_t k = pv.offsets[t]; k < pv.ids.size(); ++k) if (pv.ids[k] == id) { dup = true; break; }
				if (!dup) pv.ids.push_back(id);
			}
		}
		pv.offsets[n] = (uint32_t)pv.ids.size();
	}

	bool FacetModel::rebuildPanes(const std::vector<PaneSpec>& panes, const std::vector<char>& changed, pfc::string8& error) {
		if (panes.size() != m_panes.size() || changed.size() != panes.size()) return false;
		std::vector<ColumnJob> cols;
		bool anyFolder = false;
		if (!compileColumns(panes, changed, cols, anyFolder, error)) return false;
		const size_t n = m_tracks.get_count();
		std::vector<std::vector<pfc::string8>> raw(cols.size(), std::vector<pfc::string8>(n));
		if (!cols.empty()) {
			auto db = metadb_v2::get();
			db->queryMultiParallel_(m_tracks, [&](size_t idx, const metadb_v2::rec_t& rec) {
				for (size_t k = 0; k < cols.size(); ++k)
					db->formatTitle_v2(m_tracks[idx], rec, nullptr, raw[k][idx], cols[k].script, nullptr);
			});
		}
		if (anyFolder) ensureLocations();
		std::vector<size_t> which;
		for (size_t p = 0; p < panes.size(); ++p) if (changed[p]) which.push_back(p);
		buildPanes(panes, which, cols, raw);
		return true;
	}

	bool FacetModel::build(metadb_handle_list_cref tracks, const std::vector<PaneSpec>& panes, bool wantCreated, const std::vector<StatDef>& customs, pfc::string8& error) {
		auto compiler = titleformat_compiler::get();
		std::vector<ColumnJob> cols;
		bool anyFolder = false;
		if (!compileColumns(panes, std::vector<char>(), cols, anyFolder, error)) return false;
		titleformat_object::ptr infoScript;
		{
			pfc::string8 s;
			const char* fields[] = { "%length_seconds_fp%", "%filesize%", "[%bitrate%]", "[%added%]", "[%first_played%]",
				"[%last_played%]", "[%date%]", "[%last_modified%]", "[%play_count%]", "[$if2(%rating%,$meta(rating))]",
				"$info(encoding)", "[%replaygain_track_peak%]" };
			for (size_t k = 0; k < PFC_TABSIZE(fields); ++k) { if (k) s.add_byte(kInfoSep); s << fields[k]; }
			compiler->compile_safe(infoScript, s);
		}

		// custom statistics: a pattern that doesn't compile just gives no numbers
		std::vector<titleformat_object::ptr> customScripts(customs.size());
		m_customCombine.clear();
		for (size_t k = 0; k < customs.size(); ++k) {
			if (!compiler->compile(customScripts[k], customs[k].pattern)) customScripts[k].release();
			m_customCombine.push_back(customs[k].combine);
		}

		m_tracks = tracks;
		const size_t n = tracks.get_count();
		m_custom.assign(customs.size(), std::vector<double>(n, NAN));
		std::vector<std::vector<pfc::string8>> raw(cols.size(), std::vector<pfc::string8>(n));
		std::vector<pfc::string8> rawInfo(n);
		std::vector<uint64_t> created(wantCreated ? n : 0, 0);
		auto db = metadb_v2::get();
		db->queryMultiParallel_(tracks, [&](size_t idx, const metadb_v2::rec_t& rec) {
			for (size_t k = 0; k < cols.size(); ++k)
				db->formatTitle_v2(tracks[idx], rec, nullptr, raw[k][idx], cols[k].script, nullptr);
			db->formatTitle_v2(tracks[idx], rec, nullptr, rawInfo[idx], infoScript, nullptr);
			if (!customScripts.empty()) {
				pfc::string8 v;
				for (size_t k = 0; k < customScripts.size(); ++k) {
					if (customScripts[k].is_empty()) continue;
					db->formatTitle_v2(tracks[idx], rec, nullptr, v, customScripts[k], nullptr);
					m_custom[k][idx] = parseNumber(v);
				}
			}
			if (wantCreated) {
				// foobar2000 keeps each file's creation time with its cached stats
				if (rec.info.is_valid()) created[idx] = rec.info->stats2_().m_timestampCreate;
				if (created[idx] == 0 || created[idx] == filetimestamp_invalid) created[idx] = readCreated(tracks[idx]->get_path());
			}
		});

		m_folderOf.clear();
		m_folders.clear();
		m_locationsReady = false;
		if (anyFolder) ensureLocations();

		m_panes.assign(panes.size(), PaneValues());
		std::vector<size_t> all(panes.size());
		for (size_t p = 0; p < panes.size(); ++p) all[p] = p;
		buildPanes(panes, all, cols, raw);

		m_info.assign(n, TrackInfo());
		m_text.assign(1, pfc::string8());
		std::unordered_map<std::string, uint32_t> textIds;
		auto textId = [&](const char* v, size_t len) -> uint32_t {
			if (len == 0) return 0;
			std::string key(v, len);
			auto it = textIds.find(key);
			if (it != textIds.end()) return it->second;
			const uint32_t id = (uint32_t)m_text.size();
			m_text.push_back(pfc::string8(v, len));
			textIds.emplace(std::move(key), id);
			return id;
		};
		for (size_t t = 0; t < n; ++t) {
			std::vector<pfc::string8> f;
			const char* s = rawInfo[t].c_str();
			const char* start = s;
			for (const char* c = s;; ++c) {
				if (*c == kInfoSep || *c == 0) { f.emplace_back(start, c - start); if (*c == 0) break; start = c + 1; }
			}
			f.resize(12);
			auto& ti = m_info[t];
			ti.length = atof(f[0]);
			ti.size = _strtoui64(f[1], nullptr, 10);
			ti.bitrate = (uint32_t)strtoul(f[2], nullptr, 10);
			ti.added = textId(f[3], f[3].length()); ti.firstPlayed = textId(f[4], f[4].length()); ti.lastPlayed = textId(f[5], f[5].length());
			ti.date = textId(f[6], f[6].length()); ti.modified = textId(f[7], f[7].length());
			ti.plays = (uint32_t)strtoul(f[8], nullptr, 10);
			ti.rating = (uint32_t)strtoul(f[9], nullptr, 10);
			ti.lossless = pfc::stringEqualsI_ascii(f[10], "lossless");
			ti.peak = f[11].is_empty() ? -1 : atof(f[11]);
		}
		if (wantCreated) for (size_t t = 0; t < n; ++t) { const pfc::string8 c = formatFileTime(created[t]); m_info[t].created = textId(c, c.length()); }
		return true;
	}

	void FacetModel::accumulate(FacetItem& it, uint32_t t) const {
		const auto& ti = m_info[t];
		auto minStr = [&](int32_t& slot, uint32_t v, uint32_t TrackInfo::* field) {
			if (v == 0) return;
			if (slot < 0) { slot = (int32_t)t; return; }
			const uint32_t cur = m_info[slot].*field;
			if (v != cur && strcmp(text(v), text(cur)) < 0) slot = (int32_t)t;
		};
		auto maxStr = [&](int32_t& slot, uint32_t v, uint32_t TrackInfo::* field) {
			if (v == 0) return;
			if (slot < 0) { slot = (int32_t)t; return; }
			const uint32_t cur = m_info[slot].*field;
			if (v != cur && strcmp(text(v), text(cur)) > 0) slot = (int32_t)t;
		};
		it.count++;
		it.duration += ti.length;
		it.size += ti.size;
		if (ti.bitrate) { it.bitrateSum += ti.bitrate; it.bitrateN++; }
		minStr(it.firstAdded, ti.added, &TrackInfo::added);
		maxStr(it.lastAdded, ti.added, &TrackInfo::added);
		minStr(it.firstPlayed, ti.firstPlayed, &TrackInfo::firstPlayed);
		maxStr(it.lastPlayed, ti.lastPlayed, &TrackInfo::lastPlayed);
		minStr(it.firstRelease, ti.date, &TrackInfo::date);
		maxStr(it.lastRelease, ti.date, &TrackInfo::date);
		maxStr(it.lastModified, ti.modified, &TrackInfo::modified);
		minStr(it.firstCreated, ti.created, &TrackInfo::created);
		it.plays += ti.plays;
		it.playedDuration += ti.plays * ti.length;
		if (ti.rating) { it.ratingSum += ti.rating; it.rated++; }
		if (ti.lossless) it.lossless++;
		if (ti.peak > it.peak) it.peak = ti.peak;
		if (!m_custom.empty()) {
			if (it.custom.size() != m_custom.size()) it.custom.resize(m_custom.size());
			for (size_t k = 0; k < m_custom.size(); ++k) {
				const double v = m_custom[k][t];
				if (std::isnan(v)) continue;
				auto& c = it.custom[k];
				if (c.n == 0) c.lo = c.hi = v;
				else { if (v < c.lo) c.lo = v; if (v > c.hi) c.hi = v; }
				c.sum += v;
				c.n++;
			}
		}
	}

	bool FacetModel::customValue(size_t k, const FacetItem& it, double& out) const {
		if (k >= it.custom.size() || k >= m_customCombine.size() || it.custom[k].n == 0) return false;
		const auto& c = it.custom[k];
		switch (m_customCombine[k]) {
		case Combine::Average: out = c.sum / c.n; break;
		case Combine::Lowest: out = c.lo; break;
		case Combine::Highest: out = c.hi; break;
		default: out = c.sum; break;
		}
		return true;
	}

	std::vector<FacetItem> FacetModel::group(size_t pane, const std::vector<uint32_t>& input) const {
		const auto& pv = m_panes[pane];
		std::vector<int32_t> slot(pv.names.size(), -1);
		std::vector<FacetItem> items;
		for (uint32_t t : input) {
			for (uint32_t k = pv.offsets[t]; k < pv.offsets[t + 1]; ++k) {
				uint32_t id = pv.ids[k];
				if (slot[id] < 0) {
					slot[id] = (int32_t)items.size();
					FacetItem it; it.valueId = id; it.firstTrack = t;
					items.push_back(it);
				}
				auto& it = items[slot[id]];
				accumulate(it, t);
				if (pv.deeper[t]) it.hasChildren = true;
			}
		}
		return items;
	}

	FacetItem FacetModel::totals(size_t pane, const std::vector<uint32_t>& input) const {
		FacetItem it;
		it.firstTrack = input.empty() ? 0 : input.front();
		for (uint32_t t : input) accumulate(it, t);
		return it;
	}

	void FacetModel::computeSubitems(size_t pane, const std::vector<uint32_t>& input, std::vector<FacetItem>& items) const {
		if (pane + 1 >= m_panes.size()) { for (auto& it : items) it.subitems = 0; return; }
		const auto& pv = m_panes[pane];
		const auto& next = m_panes[pane + 1];
		std::vector<int32_t> slot(pv.names.size(), -1);
		for (size_t i = 0; i < items.size(); ++i) slot[items[i].valueId] = (int32_t)i;
		std::vector<std::vector<uint32_t>> sets(items.size());
		for (uint32_t t : input) {
			for (uint32_t k = pv.offsets[t]; k < pv.offsets[t + 1]; ++k) {
				int32_t s = slot[pv.ids[k]];
				if (s < 0) continue;
				for (uint32_t j = next.offsets[t]; j < next.offsets[t + 1]; ++j) sets[s].push_back(next.ids[j]);
			}
		}
		for (size_t i = 0; i < items.size(); ++i) {
			auto& v = sets[i];
			std::sort(v.begin(), v.end());
			items[i].subitems = (uint32_t)(std::unique(v.begin(), v.end()) - v.begin());
		}
	}

	void FacetModel::sortItems(size_t pane, std::vector<FacetItem>& items, int column, const std::vector<uint32_t>& stats, bool descending, int sortStat) const {
		const auto& pv = m_panes[pane];
		const int ncols = pv.parts.empty() ? 1 : (int)pv.parts.front().size();
		auto byText = [&](const FacetItem& a, const FacetItem& b, int c) {
			const auto& ka = pv.sortKeys[a.valueId], & kb = pv.sortKeys[b.valueId];
			for (int i = 0; i < (int)ka.size(); ++i) {
				int col = (i + c) % (int)ka.size();   // chosen column first, then the rest
				int r = StrCmpLogicalW(ka[col].c_str(), kb[col].c_str());
				if (r) return r;
			}
			return 0;
		};
		auto strOf = [&](int32_t idx, uint32_t TrackInfo::* f) -> const char* { return idx < 0 ? "" : text(m_info[idx].*f); };
		auto byStat = [&](const FacetItem& a, const FacetItem& b, Stat s) -> int {
			auto num = [](double x, double y) { return x < y ? -1 : x > y ? 1 : 0; };
			if (isCustomStat((uint32_t)s)) {
				// items without a number sort below every number
				const size_t k = (uint32_t)s - kCustomStatBase;
				double x = 0, y = 0;
				const bool hx = customValue(k, a, x), hy = customValue(k, b, y);
				if (hx != hy) return hx ? 1 : -1;
				return hx ? num(x, y) : 0;
			}
			switch (s) {
			case Stat::Bitrate: return num(a.bitrateN ? (double)a.bitrateSum / a.bitrateN : 0, b.bitrateN ? (double)b.bitrateSum / b.bitrateN : 0);
			case Stat::Duration: return num(a.duration, b.duration);
			case Stat::FirstAdded: return strcmp(strOf(a.firstAdded, &TrackInfo::added), strOf(b.firstAdded, &TrackInfo::added));
			case Stat::LastAdded: return strcmp(strOf(a.lastAdded, &TrackInfo::added), strOf(b.lastAdded, &TrackInfo::added));
			case Stat::FirstPlayed: return strcmp(strOf(a.firstPlayed, &TrackInfo::firstPlayed), strOf(b.firstPlayed, &TrackInfo::firstPlayed));
			case Stat::LastPlayed: return strcmp(strOf(a.lastPlayed, &TrackInfo::lastPlayed), strOf(b.lastPlayed, &TrackInfo::lastPlayed));
			case Stat::FirstRelease: return strcmp(strOf(a.firstRelease, &TrackInfo::date), strOf(b.firstRelease, &TrackInfo::date));
			case Stat::LastRelease: return strcmp(strOf(a.lastRelease, &TrackInfo::date), strOf(b.lastRelease, &TrackInfo::date));
			case Stat::LastModified: return strcmp(strOf(a.lastModified, &TrackInfo::modified), strOf(b.lastModified, &TrackInfo::modified));
			case Stat::FileCreated: return strcmp(strOf(a.firstCreated, &TrackInfo::created), strOf(b.firstCreated, &TrackInfo::created));
			case Stat::Items: return num(a.count, b.count);
			case Stat::LosslessRatio: return num(a.count ? (double)a.lossless / a.count : 0, b.count ? (double)b.lossless / b.count : 0);
			case Stat::Played: return num((double)a.plays, (double)b.plays);
			case Stat::PlayedDuration: return num(a.playedDuration, b.playedDuration);
			case Stat::Rating: return num(a.rated ? (double)a.ratingSum / a.rated : 0, b.rated ? (double)b.ratingSum / b.rated : 0);
			case Stat::Subitems: return num(a.subitems, b.subitems);
			case Stat::TotalPeak: return num(a.peak, b.peak);
			case Stat::TotalSize: return num((double)a.size, (double)b.size);
			default: return 0;   // Index follows the current order
			}
		};
		std::stable_sort(items.begin(), items.end(), [&](const FacetItem& a, const FacetItem& b) {
			int r;
			if (sortStat >= 0) {
				r = byStat(a, b, (Stat)sortStat);
				if (r == 0) r = byText(a, b, 0);
			} else if (column < ncols) r = byText(a, b, std::max(0, column));
			else {
				size_t si = (size_t)(column - ncols);
				r = si < stats.size() ? byStat(a, b, (Stat)stats[si]) : 0;
				if (r == 0) r = byText(a, b, 0);
			}
			return descending ? r > 0 : r < 0;
		});
	}

	std::vector<uint32_t> FacetModel::filter(size_t pane, const std::vector<uint32_t>& input, const std::vector<uint32_t>& ids, bool andLogic) const {
		const auto& pv = m_panes[pane];
		std::vector<char> want(pv.names.size(), 0);
		for (uint32_t id : ids) if (id < want.size()) want[id] = 1;
		std::vector<uint32_t> out;
		out.reserve(input.size());
		for (uint32_t t : input) {
			size_t hits = 0;
			for (uint32_t k = pv.offsets[t]; k < pv.offsets[t + 1]; ++k) if (want[pv.ids[k]]) hits++;
			if (andLogic ? (hits == ids.size() && hits > 0) : hits > 0) out.push_back(t);
		}
		return out;
	}

	std::vector<char> FacetModel::presentValues(size_t pane, const std::vector<uint32_t>& input) const {
		const auto& pv = m_panes[pane];
		std::vector<char> present(pv.names.size(), 0);
		for (uint32_t t : input) for (uint32_t k = pv.offsets[t]; k < pv.offsets[t + 1]; ++k) present[pv.ids[k]] = 1;
		return present;
	}

	bool FacetModel::findValue(size_t pane, const char* key, uint32_t& out) const {
		const auto& pv = m_panes[pane];
		auto it = pv.lookup.find(key);
		if (it == pv.lookup.end()) return false;
		out = it->second;
		return true;
	}

	pfc::string8 FacetModel::statText(Stat s, const FacetItem& it, size_t rowIndex) const {
		auto str = [&](int32_t idx, uint32_t TrackInfo::* f) -> pfc::string8 { return idx < 0 ? pfc::string8() : pfc::string8(text(m_info[idx].*f)); };
		char buf[64];
		if (isCustomStat((uint32_t)s)) {
			double v;
			return customValue((uint32_t)s - kCustomStatBase, it, v) ? formatNumber(v) : pfc::string8();
		}
		switch (s) {
		case Stat::Bitrate:
			if (!it.bitrateN) return "";
			sprintf_s(buf, "%u kbps", (unsigned)((it.bitrateSum + it.bitrateN / 2) / it.bitrateN));
			return buf;
		case Stat::Duration: return formatDuration(it.duration);
		case Stat::FirstAdded: return str(it.firstAdded, &TrackInfo::added);
		case Stat::LastAdded: return str(it.lastAdded, &TrackInfo::added);
		case Stat::FirstPlayed: return str(it.firstPlayed, &TrackInfo::firstPlayed);
		case Stat::LastPlayed: return str(it.lastPlayed, &TrackInfo::lastPlayed);
		case Stat::FirstRelease: return str(it.firstRelease, &TrackInfo::date);
		case Stat::LastRelease: return str(it.lastRelease, &TrackInfo::date);
		case Stat::LastModified: return str(it.lastModified, &TrackInfo::modified);
		case Stat::FileCreated: return str(it.firstCreated, &TrackInfo::created);
		case Stat::Index: return pfc::format_uint(rowIndex).get_ptr();
		case Stat::Items: return pfc::format_uint(it.count).get_ptr();
		case Stat::LosslessRatio:
			if (!it.count) return "";
			sprintf_s(buf, "%u%%", (unsigned)(100.0 * it.lossless / it.count + 0.5));
			return buf;
		case Stat::Played: return pfc::format_uint(it.plays).get_ptr();
		case Stat::PlayedDuration: return formatDuration(it.playedDuration);
		case Stat::Rating:
			if (!it.rated) return "";
			sprintf_s(buf, "%.1f", (double)it.ratingSum / it.rated);
			return buf;
		case Stat::Subitems: return pfc::format_uint(it.subitems).get_ptr();
		case Stat::TotalPeak:
			if (it.peak < 0) return "";
			sprintf_s(buf, "%.6f", it.peak);
			return buf;
		case Stat::TotalSize: return formatSize(it.size);
		default: return "";
		}
	}

	std::vector<uint32_t> FacetModel::allTracks() const {
		std::vector<uint32_t> v(m_tracks.get_count());
		for (uint32_t i = 0; i < v.size(); ++i) v[i] = i;
		return v;
	}

	metadb_handle_list FacetModel::handles(const std::vector<uint32_t>& idx) const {
		metadb_handle_list out;
		out.prealloc(idx.size());
		for (uint32_t i : idx) out.add_item(m_tracks[i]);
		return out;
	}

	pfc::string8 formatDuration(double seconds) {
		uint64_t s = (uint64_t)(seconds + 0.5);
		uint64_t d = s / 86400, h = (s / 3600) % 24, m = (s / 60) % 60, sec = s % 60;
		char buf[64];
		if (d) sprintf_s(buf, "%llud %02llu:%02llu:%02llu", d, h, m, sec);
		else if (h) sprintf_s(buf, "%llu:%02llu:%02llu", h, m, sec);
		else sprintf_s(buf, "%llu:%02llu", m, sec);
		return buf;
	}

	pfc::string8 formatSize(uint64_t bytes) {
		wchar_t buf[64];
		StrFormatByteSizeW((LONGLONG)bytes, buf, 64);
		return pfc::stringcvt::string_utf8_from_wide(buf).get_ptr();
	}

	bool FacetModel::findTrack(const metadb_handle_ptr& h, uint32_t& out) const {
		const size_t n = m_tracks.get_count();
		for (size_t t = 0; t < n; ++t) if (m_tracks[t] == h) { out = (uint32_t)t; return true; }
		return false;
	}

	std::vector<uint32_t> FacetModel::trackValues(size_t pane, uint32_t track) const {
		std::vector<uint32_t> out;
		if (pane >= m_panes.size()) return out;
		const auto& pv = m_panes[pane];
		if ((size_t)track + 1 >= pv.offsets.size()) return out;
		for (uint32_t k = pv.offsets[track]; k < pv.offsets[track + 1]; ++k) out.push_back(pv.ids[k]);
		return out;
	}
}
