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

#pragma once
#include <iostream>
#include <map>
#include <memory>
#include <algorithm>
#include <mutex>
#include <curl/curl.h>
#include <glibmm/dispatcher.h>
#include "anime.hpp"
#include "manga.hpp"
#include "anime_serializer.hpp"
#include "manga_serializer.hpp"
#include "user_info.hpp"
#include "text_util.hpp"
#include "active.hpp"

namespace MAL {

	struct CURLEscapeDeleter {
		void operator()(char* str) const {
			curl_free(str);
		}
	};

	struct CURLEasyDeleter {
		void operator()(CURL* curl) const {
			curl_easy_cleanup(curl);
		}
	};

	struct CURLShareDeleter {
		void operator()(CURLSH* share) const {
			CURLSHcode code = curl_share_cleanup(share);
			if (code != CURLSHE_OK) {
				std::cerr << "Error: Curl share cleanup: "
				          << curl_share_strerror(code) << std::endl;
			}
		}
	};

	class MAL {
		typedef std::function<void (CURL*, curl_lock_data, curl_lock_access)> lock_functor_t;
		typedef std::function<void (CURL*, curl_lock_data)> unlock_functor_t;
	public:
		MAL(std::unique_ptr<UserInfo>&& info);
		~MAL() = default;

        template <class UnaryFunction>
        UnaryFunction for_each_anime(UnaryFunction&& f) {
            std::lock_guard<std::mutex> lock(m_anime_list_mutex);
            return std::for_each(m_anime_list.begin(), m_anime_list.end(), std::forward<UnaryFunction>(f));
        }

        template <class UnaryFunction>
        UnaryFunction for_each_manga(UnaryFunction&& f) {
            std::lock_guard<std::mutex> lock(m_manga_list_mutex);
            return std::for_each(m_manga_list.begin(), m_manga_list.end(), std::forward<UnaryFunction>(f));
        }

        template <class UnaryFunction>
        UnaryFunction for_each_anime_search_result(UnaryFunction&& f) {
            std::lock_guard<std::mutex> lock(m_anime_search_results_mutex);
            return std::for_each(m_anime_search_results.begin(), m_anime_search_results.end(), std::forward<UnaryFunction>(f));
        }

        template <class UnaryFunction>
        UnaryFunction for_each_manga_search_result(UnaryFunction&& f) {
            std::lock_guard<std::mutex> lock(m_manga_search_results_mutex);
            return std::for_each(m_manga_search_results.begin(), m_manga_search_results.end(), std::forward<UnaryFunction>(f));
        }

		/** Returns the anime list for username. As slow as the
		 * internet.
		 * Safe to call from multiple threads.
		 */
		std::list<std::shared_ptr<Anime>> get_anime_list_sync();

		/** Returns the manga list for username. As slow as the
		 * internet.
		 * Safe to call from multiple threads.
		 */
		std::list<std::shared_ptr<Manga> > get_manga_list_sync();

		/** Searches MAL.net. Slow as the Internet.
		 * Safe to call from multiple threads.
		 */
		std::list<std::shared_ptr<Anime> > search_anime_sync(const std::string& terms);

		/** Searches MAL.net. Slow as the Internet.
		 * Safe to call from multiple threads.
		 */
		std::list<std::shared_ptr<Manga> > search_manga_sync(const std::string& terms);

		/** Updates MAL.net with the new anime details. As slow as the
		 * Internet.
		 * Safe to call from multiple threads.
		 */
		bool update_anime_sync(const Anime& anime);

		/** Updates MAL.net with the new manga details. As slow as the
		 * Internet.
		 * Safe to call from multiple threads.
		 */
		bool update_manga_sync(const Manga& manga);

		/** Adds an anime to the MAL.net anime list. As slow as the
		 * Internet.
		 * Safe to call from multiple threads.
		 */
		bool add_anime_sync(const Anime& anime);

		/** Adds an anime to the MAL.net anime list. As slow as the
		 * Internet.
		 * Safe to call from multiple threads.
		 */
		bool add_manga_sync(const Manga& manga);

		Glib::Dispatcher signal_anime_added;
		Glib::Dispatcher signal_manga_added;
        Glib::Dispatcher signal_anime_search_completed;
        Glib::Dispatcher signal_manga_search_completed;

		std::string get_image_sync(const MALItem& item);
		std::string get_manga_image_sync(const Manga& manga);

		typedef std::pair<lock_functor_t, unlock_functor_t> pair_lock_functor_t;

	private:
		const std::string LIST_BASE_URL          = "http://myanimelist.net/malappinfo.php?u=";
		const std::string SEARCH_BASE_URL        = "http://myanimelist.net/api/anime/search.xml?q=";
		const std::string UPDATED_BASE_URL       = "http://myanimelist.net/api/animelist/update/";
		const std::string ADD_BASE_URL           = "http://myanimelist.net/api/animelist/add/";
		const std::string MANGA_SEARCH_BASE_URL  = "http://myanimelist.net/api/manga/search.xml?q=";
		const std::string MANGA_UPDATED_BASE_URL = "http://myanimelist.net/api/mangalist/update/";
		const std::string MANGA_ADD_BASE_URL     = "http://myanimelist.net/api/mangalist/add/";

		std::unique_ptr<UserInfo> user_info;
		Glib::Dispatcher signal_run_password_dialog;
        Active active;
		void run_password_dialog();

		void involke_lock_function(CURL*, curl_lock_data, curl_lock_access);
		void involke_unlock_function(CURL*, curl_lock_data);

		void print_curl_error(CURLcode code);
		void print_curl_share_error(CURLSHcode code);
		void setup_curl_easy(CURL* easy, const std::string& url, std::string*);

        std::shared_ptr<TextUtility> text_util;
		AnimeSerializer serializer;
		MangaSerializer manga_serializer;
		std::unique_ptr<char[]> curl_ebuffer;
		std::unique_ptr<pair_lock_functor_t> share_lock_functors;
		std::map<curl_lock_data, std::unique_ptr<std::mutex>> map_mutex;
		std::unique_ptr<CURLSH, CURLShareDeleter> curl_share;
		
        std::map<std::string, std::unique_ptr<std::string> > image_cache;
        std::map<int_fast64_t, std::unique_ptr<std::string> > manga_image_cache;
        
        template <typename T>
        class MALItemComparator {
        public:
            bool operator()(const std::shared_ptr<const T>& l,const std::shared_ptr<const T>& r)
                {
                    auto season = l->series_date_begin.substr(0,7).compare(r->series_date_begin.substr(0,7));
                    if (season == 0)
                        return l->series_title.compare(r->series_title) < 0;
                    else
                        return season > 0;
                };
        };

        std::set<std::shared_ptr<Anime>, MALItemComparator<Anime> > m_anime_list;
        std::mutex                                                  m_anime_list_mutex;
        std::set<std::shared_ptr<Manga>, MALItemComparator<Manga> > m_manga_list;
        std::mutex                                                  m_manga_list_mutex;
        std::set<std::shared_ptr<Anime>, MALItemComparator<Anime> > m_anime_search_results;
        std::mutex                                                  m_anime_search_results_mutex;
        std::set<std::shared_ptr<Manga>, MALItemComparator<Manga> > m_manga_search_results;
        std::mutex                                                  m_manga_search_results_mutex;
	};
}
