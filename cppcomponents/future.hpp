#pragma once
#ifndef INCLUDE_GUARD_CPPCOMPONENTS_FUTURE_HPP_08_01_2013_
#define INCLUDE_GUARD_CPPCOMPONENTS_FUTURE_HPP_08_01_2013_

#include "events.hpp"

//#define CPPCOMPONENTS_USE_BOOST_FUTURE
#ifndef CPPCOMPONENTS_USE_BOOST_FUTURE
#include <future>


#else
#define BOOST_THREAD_PROVIDES_FUTURE
#define BOOST_THREAD_PROVIDES_SIGNATURE_PACKAGED_TASK
#define BOOST_RESULT_OF_USE_DECLTYPE
#include <boost/thread/future.hpp>
#endif

namespace cppcomponents{

#ifndef CPPCOMPONENTS_USE_BOOST_FUTURE
	namespace future_imp{
		using std::shared_future;
		using std::packaged_task;
		using std::async;
		using std::launch;
		using std::promise;
		using std::current_exception;
	
	}


#else
	namespace future_imp{
		using boost::shared_future;
		using boost::packaged_task;
		using boost::async;
		using boost::launch;
		using boost::promise;
		using boost::current_exception;
	
	}
#endif
	namespace detail{
		template<typename F, typename W, typename R>
		struct helper
		{
			F f_;
			W w_;

			helper(F f, W w)
				: f_(f)
				, w_(w)
			{
			}

			helper(const helper& other)
				: f_(other.f_)
				, w_(other.w_)
			{
			}

			helper(helper && other)
				: f_(std::move(other.f_))
				, w_(std::move(other.w_))
			{
			}

			helper& operator=(helper other)
			{

				f_ = std::move(other.f_);
				w_ = std::move(other.w_);
				return *this;
			}

			R operator()()
			{
				f_.wait();
				return w_(f_);
			}
		};

	}

	template<typename F, typename W>
	auto then(F f, W w) -> future_imp::shared_future<decltype(w(f))>
	{
		// Emulation of future.then
		// Can change to return f.then(w) when available
		future_imp::shared_future < decltype(w(f))> ret = future_imp::async(future_imp::launch::async, detail::helper<F, W, decltype(w(f))>(f, w));
		return ret;
	}
	namespace detail{

		template<class PromiseType>
		struct promise_helper{
			template<class PPromise, class PIfuture, class PF>
			static void set(PPromise& p, PIfuture& ifut, PF& f){
				p->set_value(f(ifut));
			}
			template<class PPromise, class PIfuture>
			static void set(PPromise& p, PIfuture& ifut){
				p->set_value(ifut.Get());
			}
		};
		template<>
		struct promise_helper<void>{
			template<class PPromise, class PIfuture, class PF>
			static void set(PPromise& p, PIfuture& ifut, PF& f){
				ifut.Get();
				f(ifut);
				p->set_value();
			}
			template<class PPromise, class PIfuture>
			static void set(PPromise& p, PIfuture& ifut){
				ifut.Get();
				p->set_value();
			}
		};
	}

	template<class T, class TUUID,class TUUIDDelegate>
	struct IFuture : public define_interface<TUUID>{
		typedef IFuture<T, TUUID, TUUIDDelegate> IFutureTemplate;
		typedef idelegate<void(use<IFuture<T, TUUID, TUUIDDelegate>>), TUUIDDelegate> delegate_type;
		typedef T value_type;
		T Get();
		void SetCompleted(use<delegate_type>);

		typedef typename IFutureTemplate::base_interface_t base_interface_t;

		template<class B>
		struct Interface : public cross_compiler_interface::define_unknown_interface<B, TUUID>{
			cross_compiler_interface::cross_function < Interface, 0, value_type(), cross_compiler_interface::detail::dummy_function<value_type()>> Get;
			cross_compiler_interface::cross_function < Interface, 1, void (use<delegate_type>), cross_compiler_interface::detail::dummy_function<void (use<delegate_type>)>> SetCompleted;

			Interface() : Get(this), SetCompleted(this){}

		};




		template<class CppComponentInterfaceExtrasT> struct InterfaceExtras : IFutureTemplate::template InterfaceExtrasBase<CppComponentInterfaceExtrasT>{
			template<class F>
			future_imp::shared_future < typename std::result_of < F(use<IFuture<T, TUUID, TUUIDDelegate >> )>::type> Then(F f){
				auto p = std::make_shared < future_imp::promise < typename std::result_of < F(use < IFuture<T, TUUID, TUUIDDelegate >> )>::type >> ();
				auto func = [p, f](use < IFutureTemplate> value){
					try{
						typedef detail::promise_helper < typename std::result_of < F(use < IFuture<T, TUUID, TUUIDDelegate >> )>::type> h_t;
						h_t::set(p, value, f);
					}
					catch (std::exception&){
						p->set_exception(future_imp::current_exception());
					}
				};

				this->get_interface().SetCompleted(make_delegate<delegate_type>(func));
				return p->get_future();
			}

			future_imp::shared_future <value_type> ToFuture(){
				auto p = std::make_shared<future_imp::promise<value_type>>();
				auto func = [p](use < IFutureTemplate> value){
					try{
						typedef detail::promise_helper <value_type> h_t;
						h_t::set(p, value);
					}
					catch (std::exception&){
						p->set_exception(future_imp::current_exception());
					}
				};

				this->get_interface().SetCompleted(make_delegate<delegate_type>(func));
				return p->get_future();
			}
			
			InterfaceExtras(){}
		};
	};
	template<class TIFuture, class T>
	use<TIFuture> make_ifuture(future_imp::shared_future<T> f);

	namespace detail{

		template<class TIFuture, class TFuture>

		struct ifuture_implementation
			: public cross_compiler_interface::implement_unknown_interfaces <
			ifuture_implementation<TIFuture, TFuture >,
			TIFuture::template Interface 
			>
		{
			typedef TIFuture ifuture_t;
			typedef typename ifuture_t::value_type value_type;
			typedef typename ifuture_t::delegate_type delegate_type;
			TFuture f_;

			future_imp::shared_future<void> resulting_f_; 

			value_type get(){
				return f_.get();
			}
			void set_completed(use<delegate_type> i){
				use<ifuture_t> pself(this->template get_implementation<ifuture_t::template Interface>()->get_use_interface(), true);
				auto func = [i,pself](TFuture sfuture)mutable{

					i.Invoke(pself);
					pself = nullptr;
				};
				resulting_f_ = then(f_, func);
			}


			ifuture_implementation(TFuture f) : f_(f){
				this->template get_implementation<ifuture_t::template Interface>()->Get.template set_mem_fn < ifuture_implementation,
					&ifuture_implementation::get>(this);
				this->template get_implementation<ifuture_t::template Interface>()->SetCompleted.template set_mem_fn < ifuture_implementation,
					&ifuture_implementation::set_completed>(this);
			}

			~ifuture_implementation(){

			}
		};
	}


	template<class TIFuture, class T>
	use<TIFuture> make_ifuture(future_imp::shared_future<T> f){
		std::unique_ptr < detail::ifuture_implementation < TIFuture, future_imp::shared_future<T >> > t(new  detail::ifuture_implementation < TIFuture, future_imp::shared_future < T >> (f));
		cppcomponents::use<TIFuture> fut(t->template get_implementation<TIFuture::template Interface>()->get_use_interface(), false);
		t.release();
		return fut;
	}

};


#endif