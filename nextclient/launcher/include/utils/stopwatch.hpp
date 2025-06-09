/*
 * Copyright (c) 2021 Adam D.
 * Distributed under the MIT license.
 * See accompanying file "LICENSE" or a copy at https://mit-license.org/
 */

#ifndef _A_STOPWATCH_HPP_
#define _A_STOPWATCH_HPP_

#include <type_traits>
#include <chrono>

namespace sw {

	// Seconds represented as a double
	using d_seconds			= std::chrono::duration<double, std::ratio<1>>;
	// Milliseconds represented as a double
	using d_milliseconds	= std::chrono::duration<double, std::milli>;
	// Microseconds represented as a double
	using d_microseconds	= std::chrono::duration<double, std::micro>;
	// Nanoseconds represented as a double
	using d_nanoseconds		= std::chrono::duration<double, std::nano>;

	// Represents a duration broken down into its components
	struct duration_components {
		int days{};
		int hours{};
		int minutes{};
		int seconds{};
		int milliseconds{};
		int microseconds{};
		int nanoseconds{};
	};

	// DO NOT USE! Internal helper utilities.
	namespace detail {

		// ================ Start of SFINAE hell

		template <typename T>
		struct is_chrono_time_point : std::false_type {};

		template <typename Rep, typename Period>
		struct is_chrono_time_point<std::chrono::time_point<Rep, Period>> : std::true_type {};

		template <typename T>
		inline constexpr bool is_chrono_time_point_v = is_chrono_time_point<T>::value;



		template <typename T>
		struct is_chrono_duration : std::false_type {};

		template <typename Rep, typename Period>
		struct is_chrono_duration<std::chrono::duration<Rep, Period>> : std::true_type {};

		template <typename T>
		inline constexpr bool is_chrono_duration_v = is_chrono_duration<T>::value;



		template <typename T>
		struct is_ratio : std::false_type {};

		template <std::intmax_t Num, std::intmax_t Den>
		struct is_ratio<std::ratio<Num, Den>> : std::true_type {};

		template <typename T>
		inline constexpr bool is_ratio_v = is_ratio<T>::value;



		template <typename T>
		struct is_trivial_clock :
			std::integral_constant<bool, (
				std::is_arithmetic_v<typename T::rep> &&
				is_ratio_v<typename T::period> &&
				std::is_same_v<typename T::duration, std::chrono::duration<typename T::rep, typename T::period>> &&
				is_chrono_time_point_v<typename T::time_point> &&
				std::is_same_v<decltype(T::is_steady), const bool> &&
				std::is_nothrow_invocable_r_v<typename T::time_point, decltype(T::now)>
			)>
		{};

		template <typename T>
		inline constexpr bool is_trivial_clock_v = is_trivial_clock<T>::value;

		// ================ End of SFINAE hell

		template <bool IsNegative, typename ExtractDuration, typename InputDuration, typename Integer>
		constexpr InputDuration extract_unit(Integer& dst, InputDuration t) noexcept {
			ExtractDuration extracted{};

			if constexpr (IsNegative) extracted = std::chrono::ceil<ExtractDuration>(t);
			else extracted = std::chrono::floor<ExtractDuration>(t);

			t -= std::chrono::duration_cast<InputDuration>(extracted);
			dst = static_cast<Integer>(extracted.count());

			return t;
		}

		using chrono_days = std::chrono::duration<int, std::ratio<86400>>; // Why doesn't this exist?

	}

	// Converts between duration types
	template <typename ToDuration, typename Rep, typename Period>
	[[nodiscard]] constexpr ToDuration convert_time(std::chrono::duration<Rep, Period> t) {
		if constexpr (std::is_same_v<ToDuration, duration_components>) {
			duration_components ret{};

			if (t.count() < Rep{}) {
				t = detail::extract_unit<true, detail::chrono_days>			(ret.days,			t);
				t = detail::extract_unit<true, std::chrono::hours>			(ret.hours,			t);
				t = detail::extract_unit<true, std::chrono::minutes>		(ret.minutes,		t);
				t = detail::extract_unit<true, std::chrono::seconds>		(ret.seconds,		t);
				t = detail::extract_unit<true, std::chrono::milliseconds>	(ret.milliseconds,	t);
				t = detail::extract_unit<true, std::chrono::microseconds>	(ret.microseconds,	t);
				ret.nanoseconds = static_cast<int>(t.count());
			} else {
				t = detail::extract_unit<false, detail::chrono_days>		(ret.days,			t);
				t = detail::extract_unit<false, std::chrono::hours>			(ret.hours,			t);
				t = detail::extract_unit<false, std::chrono::minutes>		(ret.minutes,		t);
				t = detail::extract_unit<false, std::chrono::seconds>		(ret.seconds,		t);
				t = detail::extract_unit<false, std::chrono::milliseconds>	(ret.milliseconds,	t);
				t = detail::extract_unit<false, std::chrono::microseconds>	(ret.microseconds,	t);
				ret.nanoseconds = static_cast<int>(t.count());
			}

			return ret;
		} else {
			static_assert(detail::is_chrono_duration_v<ToDuration>, "Invalid duration type");

			if constexpr (std::is_same_v<ToDuration, std::chrono::duration<Rep, Period>>) return t;
			else return std::chrono::duration_cast<ToDuration>(t);
		}
	}

	// Converts between duration types
	template <typename ToDuration>
	[[nodiscard]] constexpr ToDuration convert_time(const duration_components& t) {
		static_assert(!std::is_same_v<ToDuration, duration_components>, "A conversion to the same type is redundant");
		static_assert(detail::is_chrono_duration_v<ToDuration>, "Invalid duration type");

		auto ret = ToDuration{};

		ret += std::chrono::duration_cast<ToDuration>(detail::chrono_days		(t.days));
		ret += std::chrono::duration_cast<ToDuration>(std::chrono::hours		(t.hours));
		ret += std::chrono::duration_cast<ToDuration>(std::chrono::minutes		(t.minutes));
		ret += std::chrono::duration_cast<ToDuration>(std::chrono::seconds		(t.seconds));
		ret += std::chrono::duration_cast<ToDuration>(std::chrono::milliseconds	(t.milliseconds));
		ret += std::chrono::duration_cast<ToDuration>(std::chrono::microseconds	(t.microseconds));
		ret += std::chrono::duration_cast<ToDuration>(std::chrono::nanoseconds	(t.nanoseconds));

		return ret;
	}

	// Stopwatch class for measuring time. The template argument is a clock type to be used.
	template <typename MonotonicTrivialClock>
	class basic_stopwatch {
	public:
		using clock = std::enable_if_t<detail::is_trivial_clock_v<MonotonicTrivialClock>, MonotonicTrivialClock>;

		// Starts the stopwatch and returns the elapsed time. If the stopwatch has not been started yet, it starts it and returns a zero duration. If the stopwatch is paused, it resumes it. If the stopwatch is already running, it restarts it from 0 (this works as a "lap" function).
		auto start() noexcept {
			const auto now		= clock::now();
			const auto snapshot	= get_elapsed_impl(now, m_start, m_pause_start);

			if (has_value(m_pause_start)) {
				m_start += (now - m_pause_start);
				m_pause_start = zero_time_point;
			} else {
				m_start = now;
			}

			return snapshot;
		}

		// Starts the stopwatch and returns the elapsed time. If the stopwatch has not been started yet, it starts it and returns a zero duration. If the stopwatch is paused, it resumes it. If the stopwatch is already running, it restarts it from 0 (this works as a "lap" function).
		template <typename Duration>
		auto start() noexcept {
			return convert_time<Duration>(start());
		}

		// Pauses the stopwatch.
		void pause() noexcept {
			if (!is_paused()) {
				m_pause_start = clock::now();
			}
		}

		// Resets the stopwatch. It will be in a paused state with a time of 0 after this, just like a fresh instance.
		void reset() noexcept {
			*this = basic_stopwatch<clock>();
		}

		// Indicates if the stopwatch is paused.
		[[nodiscard]] auto is_paused() const noexcept {
			return has_value(m_pause_start) || !has_value(m_start);
		}

		// Returns the elapsed time.
		[[nodiscard]] auto get_elapsed() const noexcept {
			return get_elapsed_impl(clock::now(), m_start, m_pause_start);
		}

		// Returns the elapsed time.
		template <typename Duration>
		[[nodiscard]] auto get_elapsed() const noexcept {
			return convert_time<Duration>(get_elapsed_impl(clock::now(), m_start, m_pause_start));
		}

	private:

		static_assert(clock::is_steady, "Only monotonic clocks can be used");
		static_assert(detail::is_trivial_clock_v<clock>, "Clock must satisfy the requirements of TrivialClock");

		static constexpr typename clock::time_point zero_time_point{ clock::duration::zero() };

		typename clock::time_point m_start{ zero_time_point }, m_pause_start{ zero_time_point };

		static inline bool has_value(const typename clock::time_point& t) noexcept {
			return t != zero_time_point;
		}

		static inline auto get_elapsed_impl(const typename clock::time_point& now, const typename clock::time_point& start, const typename clock::time_point& pause_start) noexcept {
			if (has_value(pause_start)) {
				return pause_start - start;
			}

			if (has_value(start)) {
				return now - start;
			}

			return clock::duration::zero();
		}
	};

	// Stopwatch class for measuring time. Defaulted to using std::chrono::steady_clock.
	using stopwatch = basic_stopwatch<std::chrono::steady_clock>;
}

#endif