/*
//@HEADER
// ************************************************************************
//
//                        Kokkos v. 2.0
//              Copyright (2014) Sandia Corporation
//
// Under the terms of Contract DE-AC04-94AL85000 with Sandia Corporation,
// the U.S. Government retains certain rights in this software.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
// 1. Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// 3. Neither the name of the Corporation nor the names of the
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY SANDIA CORPORATION "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL SANDIA CORPORATION OR THE
// CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
// EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
// PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
// LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
// NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
// SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//
// Questions? Contact  H. Carter Edwards (hcedwar@sandia.gov)
//
// ************************************************************************
//@HEADER
*/


namespace Kokkos {

template<class Scalar>
struct Max {
  typedef Max reducer_type;
  typedef Scalar value_type;
  value_type min_value;
  value_type& result;

  Max(value_type& result_):result(result_) {}

  KOKKOS_INLINE_FUNCTION
  void join(value_type& dest, const value_type& src) {
    dest = dest<src?dest:src;
  }

  KOKKOS_INLINE_FUNCTION
  void join(volatile value_type& dest, const volatile value_type& src) {
    dest = dest<src?dest:src;
  }

  KOKKOS_INLINE_FUNCTION
  void init( value_type& val) {
    val = min_value;
  }
};

template<class Scalar>
struct Add {
  typedef Add reducer_type;
  typedef Scalar value_type;

  value_type& result;

  Add(value_type& result_):result(result_) {}

  KOKKOS_INLINE_FUNCTION
  void join(value_type& dest, const value_type& src) {
    dest += src;
  }

  KOKKOS_INLINE_FUNCTION
  void join(volatile value_type& dest, const volatile value_type& src) {
    dest += src;
  }

  KOKKOS_INLINE_FUNCTION
  void init( value_type& val) {
    val = value_type();
  }
};

namespace Impl {
template<class T>
struct is_reducer_type: public std::is_same<T,typename T::reducer_type> {};
}
}

namespace Kokkos {
namespace Impl {

template< class T, class ReturnType , class ValueTraits>
struct ParallelReduceReturnValue;

template< class ReturnType , class FunctorType >
struct ParallelReduceReturnValue<typename std::enable_if<Kokkos::is_view<ReturnType>::value>::type, ReturnType, FunctorType> {
  typedef ReturnType return_type;

  typedef typename return_type::value_type value_type_scalar;
  typedef typename return_type::value_type value_type_array[];

  typedef typename if_c<return_type::rank==0,value_type_scalar,value_type_array>::type value_type;

  static return_type& return_value(ReturnType& return_val, const FunctorType&) {
    return return_val;
  }
};

template< class ReturnType , class FunctorType>
struct ParallelReduceReturnValue<typename std::enable_if<
                                   !Kokkos::is_view<ReturnType>::value &&
                                  (!std::is_array<ReturnType>::value && !std::is_pointer<ReturnType>::value) &&
                                   !Kokkos::is_reducer_type<ReturnType>::value
                                 >::type, ReturnType, FunctorType> {
  typedef Kokkos::View<  ReturnType
                       , Kokkos::HostSpace
                       , Kokkos::MemoryUnmanaged
      > return_type;
  typedef typename return_type::value_type value_type;

  static return_type return_value(ReturnType& return_val, const FunctorType&) {
    return return_type(&return_val);
  }
};

template< class ReturnType , class FunctorType>
struct ParallelReduceReturnValue<typename std::enable_if<
                                  (is_array<ReturnType>::value || std::is_pointer<ReturnType>::value)
                                >::type, ReturnType, FunctorType> {
  typedef Kokkos::View<  ReturnType
                       , Kokkos::HostSpace
                       , Kokkos::MemoryUnmanaged
      > return_type;
  typedef typename return_type::value_type value_type[];

  static return_type return_value(ReturnType& return_val,
                                  const FunctorType& functor) {
    return return_type(return_val,functor.value_count);
  }
};

template< class ReturnType , class FunctorType>
struct ParallelReduceReturnValue<typename std::enable_if<
                                   Kokkos::is_reducer_type<ReturnType>::value
                                >::type, ReturnType, FunctorType> {
  typedef Kokkos::View<  typename ReturnType::value_type
                       , Kokkos::HostSpace
                       , Kokkos::MemoryUnmanaged
      > return_type;
  typedef typename return_type::value_type value_type;

  static return_type return_value(ReturnType& return_val,
                                  const FunctorType& functor) {
    return return_type(&return_val.result);
  }
};
}

namespace Impl {
template< class T, class ReturnType , class FunctorType>
struct ParallelReducePolicyType;

template< class PolicyType , class FunctorType >
struct ParallelReducePolicyType<typename std::enable_if<Kokkos::Impl::is_execution_policy<PolicyType>::value>::type, PolicyType,FunctorType> {

  typedef PolicyType policy_type;
  static PolicyType policy(const PolicyType& policy_) {
    return policy_;
  }
};

template< class PolicyType , class FunctorType >
struct ParallelReducePolicyType<typename std::enable_if<std::is_integral<PolicyType>::value>::type, PolicyType,FunctorType> {
  typedef typename
    Impl::FunctorPolicyExecutionSpace< FunctorType , void >::execution_space
      execution_space ;

  typedef Kokkos::RangePolicy<execution_space> policy_type;

  static policy_type policy(const PolicyType& policy_) {
    return policy_type(0,policy_);
  }
};

}

namespace Impl {
  template< class FunctorType, class ExecPolicy, class ValueType, class ExecutionSpace>
  struct ParallelReduceFunctorType {
    typedef FunctorType functor_type;
    static const functor_type& functor(const functor_type& functor) {
      return functor;
    }
  };
}

namespace Impl {
  template< class PolicyType, class FunctorType, class ReturnType >
  struct ParallelReduceAdaptor {
    typedef Impl::ParallelReduceReturnValue<void,ReturnType,FunctorType> return_value_adapter;
    typedef Impl::ParallelReduceFunctorType<FunctorType,PolicyType,
                                            typename return_value_adapter::value_type,
                                            typename PolicyType::execution_space> functor_adaptor;

    static inline
    void execute(const std::string& label,
        const PolicyType& policy,
        const FunctorType& functor,
        ReturnType& return_value) {
          #if (KOKKOS_ENABLE_PROFILING)
            uint64_t kpID = 0;
            if(Kokkos::Profiling::profileLibraryLoaded()) {
              Kokkos::Profiling::beginParallelReduce(std::string(label), 0, &kpID);
            }
          #endif

          Kokkos::Impl::shared_allocation_tracking_claim_and_disable();
          Impl::ParallelReduce<typename functor_adaptor::functor_type, PolicyType >
             closure(functor_adaptor::functor(functor),
                     policy,
                     return_value_adapter::return_value(return_value,functor));
          Kokkos::Impl::shared_allocation_tracking_release_and_enable();
          closure.execute();

          #if (KOKKOS_ENABLE_PROFILING)
            if(Kokkos::Profiling::profileLibraryLoaded()) {
              Kokkos::Profiling::endParallelReduce(kpID);
            }
          #endif
        }

  };
}
template< class PolicyType, class FunctorType, class ReturnType >
inline
void parallel_reduce(const std::string& label,
                     const PolicyType& policy,
                     const FunctorType& functor,
                     ReturnType& return_value,
                     typename Impl::enable_if<
                       Kokkos::Impl::is_execution_policy<PolicyType>::value
                     >::type * = 0) {
  Impl::ParallelReduceAdaptor<PolicyType,FunctorType,ReturnType>::execute(label,policy,functor,return_value);
}

template< class PolicyType, class FunctorType, class ReturnType >
inline
void parallel_reduce(const PolicyType& policy,
                     const FunctorType& functor,
                     ReturnType& return_value,
                     typename Impl::enable_if<
                       Kokkos::Impl::is_execution_policy<PolicyType>::value
                     >::type * = 0) {
  Impl::ParallelReduceAdaptor<PolicyType,FunctorType,ReturnType>::execute("No Label",policy,functor,return_value);
}

template< class FunctorType, class ReturnType >
inline
void parallel_reduce(const size_t& policy,
                     const FunctorType& functor,
                     ReturnType& return_value) {
  typedef typename Impl::ParallelReducePolicyType<void,size_t,FunctorType>::policy_type policy_type;
  Impl::ParallelReduceAdaptor<policy_type,FunctorType,ReturnType>::execute("No Label",policy_type(0,policy),functor,return_value);
}

template< class FunctorType, class ReturnType >
inline
void parallel_reduce(const std::string& label,
                     const size_t& policy,
                     const FunctorType& functor,
                     ReturnType& return_value) {
  typedef typename Impl::ParallelReducePolicyType<void,size_t,FunctorType>::policy_type policy_type;
  Impl::ParallelReduceAdaptor<policy_type,FunctorType,ReturnType>::execute(label,policy_type(0,policy),functor,return_value);
}

} //namespace Kokkos