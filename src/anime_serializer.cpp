/*
 *  This file is part of mal-gtk.
 *
 *  mal-gtk is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  mal-gtk is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with mal-gtk.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <iostream>
#include <cstring>
#include <memory>
#include <libxml/xmlreader.h>
#include "anime_serializer.hpp"

namespace MAL {
	enum FIELDS : int_fast8_t { FIELDNONE, FIELDTEXT,
			ANIMEDBID, SERIESTITLE, SERIESTYPE, SERIESEPISODES, SERIESSTATUS,
			SERIESDATEBEGIN, SERIESDATEEND, SERIESIMAGEURL, SERIESSYNONYMS,
			ANIME, ENTRY, MYID, MYWATCHEDEPISODES, MYSTARTDATE, MYFINISHDATE, MYSCORE,
			MYSTATUS, MYREWATCHING, MYREWATCHINGEP, MYLASTUPDATED, MYTAGS,
			USERID, SYNOPSIS
			};
}

namespace {
	static std::map<const std::string, const MAL::FIELDS> initialize_field_map() {
		return {
			{ "my_id",                    MAL::MYID },
			{ "series_animedb_id",        MAL::ANIMEDBID },
			{ "id",                       MAL::ANIMEDBID },
			{ "series_title",             MAL::SERIESTITLE },
			{ "title",                    MAL::SERIESTITLE },
			{ "series_type",              MAL::SERIESTYPE },
			{ "type",                     MAL::SERIESTYPE },
			{ "series_episodes",          MAL::SERIESEPISODES },
			{ "episodes",                 MAL::SERIESEPISODES },
			{ "series_status",            MAL::SERIESSTATUS },
			{ "status",                   MAL::SERIESSTATUS },
			{ "series_start",             MAL::SERIESDATEBEGIN },
			{ "start_date",               MAL::SERIESDATEBEGIN },
			{ "series_end",               MAL::SERIESDATEEND },
			{ "end_date",                 MAL::SERIESDATEEND },
			{ "series_image",             MAL::SERIESIMAGEURL },
			{ "image",                    MAL::SERIESIMAGEURL },
			{ "series_synonyms",          MAL::SERIESSYNONYMS },
			{ "synonyms",                 MAL::SERIESSYNONYMS },
			{ "my_score",                 MAL::MYSCORE },
			{ "score",                    MAL::MYSCORE },
			{ "english",                  MAL::SERIESSYNONYMS },
			{ "my_watched_episodes",      MAL::MYWATCHEDEPISODES },
			{ "my_start_date",            MAL::MYSTARTDATE },
			{ "my_finish_date",           MAL::MYFINISHDATE },
			{ "my_status",                MAL::MYSTATUS },
			{ "my_rewatching",            MAL::MYREWATCHING },
			{ "my_rewatching_ep",         MAL::MYREWATCHINGEP },
			{ "my_last_updated",          MAL::MYLASTUPDATED },
			{ "my_tags",                  MAL::MYTAGS },
			{ "user_id",                  MAL::USERID },
			{ "synopsis",                 MAL::SYNOPSIS },
			{ "entry",                    MAL::ENTRY },
			{ "anime",                    MAL::ANIME },
			{ "#text",                    MAL::FIELDTEXT },
			{ "user_name",                MAL::FIELDNONE },
			{ "user_watching",            MAL::FIELDNONE },
			{ "user_completed",           MAL::FIELDNONE },
			{ "user_onhold",              MAL::FIELDNONE },
			{ "user_dropped",             MAL::FIELDNONE },
			{ "user_plantowatch",         MAL::FIELDNONE },
			{ "user_days_spent_watching", MAL::FIELDNONE },
			{ "myinfo",                   MAL::FIELDNONE },
			{ "myanimelist",              MAL::FIELDNONE },
		};
	}

	static std::map<const MAL::FIELDS, std::function<void (MAL::Anime&, std::string&&)> > initialize_member_map() {
		return {
			{ MAL::ANIMEDBID,         std::mem_fn(&MAL::Anime::set_series_itemdb_id) },
			{ MAL::SERIESTITLE,       std::mem_fn(&MAL::Anime::set_series_title) },
			{ MAL::SERIESDATEBEGIN,   std::mem_fn(&MAL::Anime::set_series_date_begin) },
			{ MAL::SERIESDATEEND,     std::mem_fn(&MAL::Anime::set_series_date_end) },
			{ MAL::SERIESIMAGEURL,    std::mem_fn(&MAL::Anime::set_image_url) },
			{ MAL::SERIESSYNONYMS,    std::mem_fn(&MAL::Anime::set_series_synonyms) },
			{ MAL::SYNOPSIS,          std::mem_fn(&MAL::Anime::set_series_synopsis) },

			{ MAL::SERIESTYPE,        std::mem_fn(&MAL::Anime::set_series_type) },
			{ MAL::SERIESSTATUS,      std::mem_fn(&MAL::Anime::set_series_status) },
			{ MAL::SERIESEPISODES,    std::mem_fn(&MAL::Anime::set_series_episodes) },

			{ MAL::MYTAGS,            std::mem_fn(&MAL::Anime::set_tags) },
			{ MAL::MYSTARTDATE,       std::mem_fn(&MAL::Anime::set_date_start) },
			{ MAL::MYFINISHDATE,      std::mem_fn(&MAL::Anime::set_date_finish) },
			{ MAL::MYID,              std::mem_fn(&MAL::Anime::set_id) },
			{ MAL::MYLASTUPDATED,     std::mem_fn(&MAL::Anime::set_last_updated) },
			{ MAL::MYSCORE,           std::mem_fn(&MAL::Anime::set_score) },
			{ MAL::MYREWATCHING,      std::mem_fn(&MAL::Anime::set_enable_reconsuming) },

			{ MAL::MYSTATUS,          std::mem_fn(&MAL::Anime::set_status) },
			{ MAL::MYWATCHEDEPISODES, std::mem_fn(&MAL::Anime::set_episodes) },
			{ MAL::MYREWATCHINGEP,    std::mem_fn(&MAL::Anime::set_rewatch_episode) },
		};
	}

	struct xmlTextReaderDeleter {
		void operator()(xmlTextReaderPtr reader) const {
			xmlFreeTextReader(reader);
		}
	};

	static std::string xmlchar_to_str(const xmlChar* str) {
		if (str)
			return std::string(reinterpret_cast<const char*>(str));
		else
			return std::string();
	}
}

namespace MAL {

	AnimeSerializer::AnimeSerializer(const std::shared_ptr<TextUtility>& text_util) :
		field_map(initialize_field_map()),
		member_map(initialize_member_map()),
        m_text_util(text_util)
	{
	}


	/** Handles deserialization from both myanimelist and search results.
	 * Search results are      <anime><entry></entry><entry></entry></anime>
	 * myanimelist results are <entry><anime></anime><anime></anime></entry>
	 */
	std::list<std::shared_ptr<Anime> > AnimeSerializer::deserialize(const std::string& xml) const {
		std::list<std::shared_ptr<Anime> > res;
		std::unique_ptr<char[]> cstr(new char[xml.size()]);
		std::memcpy(cstr.get(), xml.c_str(), xml.size());
		std::unique_ptr<xmlTextReader, xmlTextReaderDeleter> reader(xmlReaderForMemory(
			         cstr.get(), xml.size(), "", "UTF-8", XML_PARSE_RECOVER | XML_PARSE_NOENT ));

		if (!reader) {
			std::cerr << "Error: Couldn't create XML reader" << std::endl;
            std::cerr << "XML follows: " << xml << std::endl;
			return res;
		}

		Anime anime;
		int ret                = 1;
		FIELDS field           = FIELDNONE;
		FIELDS prev_field      = FIELDNONE;
		bool entry_after_anime = false;
		bool seen_anime        = false;
		bool seen_entry        = false;
		for( ret = xmlTextReaderRead(reader.get()); ret == 1;
		     ret = xmlTextReaderRead(reader.get()) ) {
			const std::string name  = xmlchar_to_str(xmlTextReaderConstName (reader.get()));
			std::string value = xmlchar_to_str(xmlTextReaderConstValue(reader.get()));
            m_text_util->parse_html_entities(value);

			if (name.size() > 0) {
				auto field_iter = field_map.find(name);
				if (field_iter != field_map.end()) {
					prev_field = field;
					field = field_iter->second;
				} else {
					std::cerr << "Unexpected field " << name << std::endl;
				}

				switch (xmlTextReaderNodeType(reader.get())) {
				case XML_READER_TYPE_ELEMENT:
					entry_after_anime |= (field == ENTRY) && seen_anime && !seen_entry;
					seen_entry        |= field == ENTRY;
					seen_anime        |= field == ANIME;
					break;
				case XML_READER_TYPE_END_ELEMENT:
					if ( ( entry_after_anime && field == ENTRY) ||
					     (!entry_after_anime && field == ANIME)) {
						res.push_back(std::make_shared<Anime>(anime));
						anime = Anime();
					}
					field = FIELDNONE;
					break;
				case XML_READER_TYPE_TEXT:
					if( field != FIELDTEXT ) {
						std::cerr << "There's a problem!" << std::endl;
					}
					if (value.size() > 0) {
						auto member_iter = member_map.find(prev_field);
						if ( member_iter != member_map.end() ) {
							member_iter->second(anime, std::move(value));
						}
					} else {
						std::cerr << "Error: Unexpected " << name << " = " 
						          << value << std::endl;
					}
					break;
				case XML_READER_TYPE_SIGNIFICANT_WHITESPACE:
					break;
				default:
					std::cerr << "Warning: Unexpected node type "
					          << xmlTextReaderNodeType(reader.get())
					          << " : " << name << "=" << value << std::endl;
					break;
				}
			}
		}
		
		if ( ret != 0 ) {
			std::cerr << "Error: Failed to parse! ret = " << ret << std::endl;
		}

		return res;
	}

	std::string AnimeSerializer::serialize(const Anime& anime) const {
		std::string out;
		
		out += "<?xml version=\"1.0\" encoding=\"UTF-8\"?><entry>";
		out += "<episode>";
		out += std::to_string(anime.episodes);
		out += "</episode>";
		out += "<status>";
		out += std::to_string(anime.status);
		out += "</status>";
		out += "<score>";
		out += std::to_string(anime.score);
		out += "</score>";
		out += "<downloaded_episodes>";
		out += std::to_string(anime.downloaded_items);
		out += "</downloaded_episodes>";
		out += "<storage_type>";
		//out += std::to_string(anime.storage_type);
		out += "</storage_type>";
		out += "<storage_value>";
		//out += std::to_string(anime.storage_value);
		out += "</storage_value>";
		out += "<times_rewatched>";
		//out += std::to_string(anime.times_consumed);
		out += "</times_rewatched>";
		out += "<rewatch_value>";
		//out += std::to_string(anime.reconsume_value);
		out += "</rewatch_value>";
		out += "<date_start>";
        auto start = Glib::Date();
        start.set_parse(anime.date_start);
        if (start.valid()) {
            out += start.format_string("%m%d%Y");
        }
		out += "</date_start>";
		out += "<date_finish>";
        auto finish = Glib::Date();
        finish.set_parse(anime.date_finish);
        if (finish.valid()) {
            out += finish.format_string("%m%d%Y");
        }
		out += "</date_finish>";
		out += "<priority>";
		//out += std::to_string(anime.priority);
		out += "</priority>";
		out += "<enable_discussion>";
		//out += anime.enable_discussion?"1":"0";
		out += "</enable_discussion>";
		out += "<enable_rewatching>";
		out += (anime.enable_reconsuming)?"1":"0";
		out += "</enable_rewatching>";
		out += "<comments>";
		//out += anime.comments;
		out += "</comments>";
		out += "<fansub_group>";
		//out += std::to_string(anime.fansub_group);
		out += "</fansub_group>";
		out += "<tags>";
		auto iter = anime.tags.begin();
		bool was_first = true;
		while (iter != anime.tags.end()) {
			if (!was_first)
				out += "; ";
			out += *iter;
			was_first = false;
			++iter;
		}
		out += "</tags>";
		out += "<rewatch_episode>";
		out += std::to_string(anime.rewatch_episode);
		out += "</rewatch_episode>";
		out += "</entry>";
		return out;
	}
	
}
