/**
 * Autogenerated by Thrift Compiler (0.18.1)
 *
 * DO NOT EDIT UNLESS YOU ARE SURE THAT YOU KNOW WHAT YOU ARE DOING
 *  @generated
 */
#ifndef mapreduce_TYPES_H
#define mapreduce_TYPES_H

#include <iosfwd>

#include <thrift/Thrift.h>
#include <thrift/TApplicationException.h>
#include <thrift/TBase.h>
#include <thrift/protocol/TProtocol.h>
#include <thrift/transport/TTransport.h>

#include <functional>
#include <memory>




struct ResponseType {
  enum type {
    WAIT = 0,
    MAP_TASK = 1,
    REDUCE_TASK = 2,
    COMPLETED = 3
  };
};

extern const std::map<int, const char*> _ResponseType_VALUES_TO_NAMES;

std::ostream& operator<<(std::ostream& out, const ResponseType::type& val);

std::string to_string(const ResponseType::type& val);

class TaskResponse;

class TaskResult;

typedef struct _TaskResponse__isset {
  _TaskResponse__isset() : id(false), type(false), params(false), resultNum(false) {}
  bool id :1;
  bool type :1;
  bool params :1;
  bool resultNum :1;
} _TaskResponse__isset;

class TaskResponse : public virtual ::apache::thrift::TBase {
 public:

  TaskResponse(const TaskResponse&);
  TaskResponse& operator=(const TaskResponse&);
  TaskResponse() noexcept
               : id(0),
                 type(static_cast<ResponseType::type>(0)),
                 resultNum(0) {
  }

  virtual ~TaskResponse() noexcept;
  int32_t id;
  /**
   * 
   * @see ResponseType
   */
  ResponseType::type type;
  std::vector<std::string>  params;
  int32_t resultNum;

  _TaskResponse__isset __isset;

  void __set_id(const int32_t val);

  void __set_type(const ResponseType::type val);

  void __set_params(const std::vector<std::string> & val);

  void __set_resultNum(const int32_t val);

  bool operator == (const TaskResponse & rhs) const
  {
    if (!(id == rhs.id))
      return false;
    if (!(type == rhs.type))
      return false;
    if (!(params == rhs.params))
      return false;
    if (!(resultNum == rhs.resultNum))
      return false;
    return true;
  }
  bool operator != (const TaskResponse &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const TaskResponse & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot) override;
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const override;

  virtual void printTo(std::ostream& out) const;
};

void swap(TaskResponse &a, TaskResponse &b);

std::ostream& operator<<(std::ostream& out, const TaskResponse& obj);

typedef struct _TaskResult__isset {
  _TaskResult__isset() : id(false), type(false), result_location(false) {}
  bool id :1;
  bool type :1;
  bool result_location :1;
} _TaskResult__isset;

class TaskResult : public virtual ::apache::thrift::TBase {
 public:

  TaskResult(const TaskResult&);
  TaskResult& operator=(const TaskResult&);
  TaskResult() noexcept
             : id(0),
               type(static_cast<ResponseType::type>(0)) {
  }

  virtual ~TaskResult() noexcept;
  int32_t id;
  /**
   * 
   * @see ResponseType
   */
  ResponseType::type type;
  std::vector<std::string>  result_location;

  _TaskResult__isset __isset;

  void __set_id(const int32_t val);

  void __set_type(const ResponseType::type val);

  void __set_result_location(const std::vector<std::string> & val);

  bool operator == (const TaskResult & rhs) const
  {
    if (!(id == rhs.id))
      return false;
    if (!(type == rhs.type))
      return false;
    if (!(result_location == rhs.result_location))
      return false;
    return true;
  }
  bool operator != (const TaskResult &rhs) const {
    return !(*this == rhs);
  }

  bool operator < (const TaskResult & ) const;

  uint32_t read(::apache::thrift::protocol::TProtocol* iprot) override;
  uint32_t write(::apache::thrift::protocol::TProtocol* oprot) const override;

  virtual void printTo(std::ostream& out) const;
};

void swap(TaskResult &a, TaskResult &b);

std::ostream& operator<<(std::ostream& out, const TaskResult& obj);



#endif
