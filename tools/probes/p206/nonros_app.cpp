// Throwaway probe for issue #206: a standalone Fast DDS 3.x application that uses NO
// rclcpp / rmw at all, built straight against libfastdds. It publishes on the ROS 2
// topic "rt/chatter" with the ROS 2 type name "std_msgs::msg::dds_::String_" but with
// two different member sets, so that we can see (a) whether a non-ROS peer announces
// XTypes TypeInformation, (b) whether EK_COMPLETE is populated, and (c) whether the
// equivalence hashes differ for two different definitions of one type name.
//
// Build inside the image (no ROS packages, no ament):
//   g++ -std=c++17 -O1 nonros_app.cpp -o nonros_app \
//       -I/opt/ros/lyrical/include/fastdds -I/opt/ros/lyrical/includefastcdr \
//       -L/opt/ros/lyrical/lib -L/opt/ros/lyrical/lib/aarch64-linux-gnu \
//       -lfastdds -lfastcdr -pthread
//   (note the image really does ship the headers in "include" + "fastcdr" concatenated)
//
// usage: nonros_app <seconds> [ab]   env:
//   ROS_DOMAIN_ID, P206_TOPIC (rt/chatter), P206_TYPE_NAME (example_interfaces::msg::dds_::String_),
//   P206_EXT=final|appendable|mutable, P206_MEMBER_NAME (data), P206_NO_TYPEOBJECT=1
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/DataWriterListener.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicData.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicDataFactory.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicPubSubType.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicType.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicTypeBuilder.hpp>
#include <fastdds/dds/xtypes/dynamic_types/DynamicTypeBuilderFactory.hpp>
#include <fastdds/dds/xtypes/dynamic_types/MemberDescriptor.hpp>
#include <fastdds/dds/xtypes/dynamic_types/TypeDescriptor.hpp>

using namespace eprosima::fastdds::dds;

static std::string kTypeName = "example_interfaces::msg::dds_::String_";
static std::string kTopic = "rt/chatter";

// struct <kTypeName> { string data; [int32 extra;] }
static DynamicType::_ref_type build_string_type(bool with_extra)
{
  auto factory = DynamicTypeBuilderFactory::get_instance();
  TypeDescriptor::_ref_type td{traits<TypeDescriptor>::make_shared()};
  td->kind(TK_STRUCTURE);
  td->name(kTypeName.c_str());
  // P206_EXT=final|appendable|mutable (default: the DynamicTypeBuilder default)
  if (const char * e = std::getenv("P206_EXT")) {
    const std::string ek{e};
    if (ek == "final") { td->extensibility_kind(ExtensibilityKind::FINAL); }
    else if (ek == "appendable") { td->extensibility_kind(ExtensibilityKind::APPENDABLE); }
    else if (ek == "mutable") { td->extensibility_kind(ExtensibilityKind::MUTABLE); }
  }
  DynamicTypeBuilder::_ref_type b{factory->create_type(td)};

  MemberDescriptor::_ref_type m0{traits<MemberDescriptor>::make_shared()};
  m0->name(std::getenv("P206_MEMBER_NAME") ? std::getenv("P206_MEMBER_NAME") : "data");
  m0->type(factory->create_string_type(static_cast<uint32_t>(LENGTH_UNLIMITED))->build());
  b->add_member(m0);

  if (with_extra) {
    MemberDescriptor::_ref_type m1{traits<MemberDescriptor>::make_shared()};
    m1->name("extra");
    m1->type(factory->get_primitive_type(TK_INT32));
    b->add_member(m1);
  }
  return b->build();
}

class WLis : public DataWriterListener
{
public:
  explicit WLis(std::string tag) : tag_(std::move(tag)) {}
  void on_publication_matched(DataWriter *, const PublicationMatchedStatus & s) override
  {
    std::cout << "# MATCH writer " << tag_ << ": current_count=" << s.current_count
              << " change=" << s.current_count_change << std::endl;
  }
  std::string tag_;
};

class RLis : public DataReaderListener
{
public:
  explicit RLis(std::string tag) : tag_(std::move(tag)) {}
  void on_subscription_matched(DataReader *, const SubscriptionMatchedStatus & s) override
  {
    std::cout << "# MATCH reader " << tag_ << ": current_count=" << s.current_count
              << " change=" << s.current_count_change << std::endl;
  }
  void on_data_available(DataReader * r) override
  {
    DynamicData::_ref_type d{DynamicDataFactory::get_instance()->create_data(type_)};
    SampleInfo info;
    while (RETCODE_OK == r->take_next_sample(&d, &info)) {
      if (info.valid_data) {
        ++received_;
        if (received_ <= 3) {
          std::cout << "# DATA reader " << tag_ << ": sample " << received_ << std::endl;
        }
      }
    }
  }
  std::string tag_;
  DynamicType::_ref_type type_;
  std::atomic<int> received_{0};
};

// A non-ROS peer that ships its own TopicDataType and never registers a TypeObject:
// TopicDataType::register_type_object_representation() is a no-op by default in 3.x.
class NoTypeObjectPubSubType : public DynamicPubSubType
{
public:
  explicit NoTypeObjectPubSubType(DynamicType::_ref_type t) : DynamicPubSubType(t) {}
  void register_type_object_representation() override {}
};

struct Side
{
  DomainParticipant * p{nullptr};
  Publisher * pub{nullptr};
  Subscriber * sub{nullptr};
  Topic * topic{nullptr};
  DataWriter * w{nullptr};
  DataReader * r{nullptr};
  DynamicType::_ref_type type;
  WLis * wl{nullptr};
  RLis * rl{nullptr};
};

static bool make_side(Side & s, int domain, const char * pname, bool with_extra, bool want_reader)
{
  auto factory = DomainParticipantFactory::get_instance();
  DomainParticipantQos pqos = PARTICIPANT_QOS_DEFAULT;
  pqos.name(pname);
  // No ROS 2 USER_DATA is set anywhere: this participant is not a ROS node.
  s.p = factory->create_participant(domain, pqos);
  if (!s.p) { std::cerr << "create_participant failed" << std::endl; return false; }

  s.type = build_string_type(with_extra);
  const bool no_to = std::getenv("P206_NO_TYPEOBJECT") != nullptr;
  TypeSupport ts(no_to
      ? static_cast<TopicDataType *>(new NoTypeObjectPubSubType(s.type))
      : static_cast<TopicDataType *>(new DynamicPubSubType(s.type)));
  if (!no_to) { ts->register_type_object_representation(); }
  if (RETCODE_OK != ts.register_type(s.p)) {
    std::cerr << "register_type failed" << std::endl; return false;
  }

  s.topic = s.p->create_topic(kTopic, kTypeName, TOPIC_QOS_DEFAULT);
  if (!s.topic) { std::cerr << "create_topic failed" << std::endl; return false; }

  s.pub = s.p->create_publisher(PUBLISHER_QOS_DEFAULT);
  DataWriterQos wqos = DATAWRITER_QOS_DEFAULT;
  wqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
  wqos.history().kind = KEEP_LAST_HISTORY_QOS;
  wqos.history().depth = 10;
  wqos.durability().kind = VOLATILE_DURABILITY_QOS;
  s.wl = new WLis(pname);
  s.w = s.pub->create_datawriter(s.topic, wqos, s.wl);
  if (!s.w) { std::cerr << "create_datawriter failed" << std::endl; return false; }

  if (want_reader) {
    s.sub = s.p->create_subscriber(SUBSCRIBER_QOS_DEFAULT);
    DataReaderQos rqos = DATAREADER_QOS_DEFAULT;
    rqos.reliability().kind = RELIABLE_RELIABILITY_QOS;
    rqos.history().kind = KEEP_LAST_HISTORY_QOS;
    rqos.history().depth = 10;
    rqos.durability().kind = VOLATILE_DURABILITY_QOS;
    s.rl = new RLis(pname);
    s.rl->type_ = s.type;
    s.r = s.sub->create_datareader(s.topic, rqos, s.rl);
    if (!s.r) { std::cerr << "create_datareader failed" << std::endl; return false; }
  }
  std::cout << "# ext_kind=" << static_cast<int>(
    (s.type ? 0 : 0)) << " (0=FINAL,1=APPENDABLE,2=MUTABLE) env P206_EXT="
            << (std::getenv("P206_EXT") ? std::getenv("P206_EXT") : "(default)") << std::endl;
  std::cout << "# up: participant=" << pname << " topic=" << kTopic << " type=" << kTypeName
            << " members=" << (with_extra ? "{string data; int32 extra;}" : "{string data;}")
            << " writer_guid_prefix_set reader=" << (want_reader ? "yes" : "no") << std::endl;
  return true;
}

int main(int argc, char ** argv)
{
  const int seconds = argc > 1 ? std::atoi(argv[1]) : 30;
  int domain = 0;
  if (const char * d = std::getenv("ROS_DOMAIN_ID")) { domain = std::atoi(d); }
  if (const char * t = std::getenv("P206_TYPE_NAME")) { kTypeName = t; }
  if (const char * t = std::getenv("P206_TOPIC")) { kTopic = t; }
  // Which sides to bring up: "a", "b", or "ab" (default).
  const std::string which = argc > 2 ? argv[2] : "ab";

  std::cout << "# non-ROS Fast DDS app, domain " << domain << ", " << seconds << "s, sides=" << which
            << std::endl;

  Side a, b;
  // side A: the ROS 2 definition of std_msgs/msg/String  => struct { string data; }
  if (which.find('a') != std::string::npos && !make_side(a, domain, "p206_nonros_a", false, true)) {
    return 1;
  }
  // side B: the SAME type name with a different member set => struct { string data; int32 extra; }
  if (which.find('b') != std::string::npos && !make_side(b, domain, "p206_nonros_b", true, true)) {
    return 1;
  }

  auto write_one = [](Side & s, int i) {
      if (!s.w) { return; }
      DynamicData::_ref_type d{DynamicDataFactory::get_instance()->create_data(s.type)};
      d->set_string_value(d->get_member_id_by_name(
        std::getenv("P206_MEMBER_NAME") ? std::getenv("P206_MEMBER_NAME") : "data"), "hello from the non-ROS app " +
      std::to_string(i));
      if (s.type->get_member_count() > 1) {
        d->set_int32_value(d->get_member_id_by_name("extra"), i);
      }
      s.w->write(&d);
    };

  for (int i = 0; i < seconds * 2; ++i) {
    write_one(a, i);
    write_one(b, i);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
  }
  std::cout << "# received: a=" << (a.rl ? a.rl->received_.load() : -1)
            << " b=" << (b.rl ? b.rl->received_.load() : -1) << std::endl;
  auto factory = DomainParticipantFactory::get_instance();
  for (Side * s : {&a, &b}) {
    if (s->p) { s->p->delete_contained_entities(); factory->delete_participant(s->p); }
  }
  return 0;
}
