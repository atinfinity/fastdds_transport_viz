#include <rclcpp/rclcpp.hpp>
#include <p213_msgs/msg/foo.hpp>
int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  auto n = rclcpp::Node::make_shared("foo_sub");
  int c = 0;
  auto s = n->create_subscription<p213_msgs::msg::Foo>("/p213", 10,
    [&](const p213_msgs::msg::Foo & m) { RCLCPP_INFO(n->get_logger(), "RECV %d s=%s", ++c, m.s.c_str()); });
  auto t = n->create_wall_timer(std::chrono::seconds(1), [&]() {
    RCLCPP_INFO(n->get_logger(), "sub matched=%zu recv=%d", s->get_publisher_count(), c); });
  rclcpp::spin(n); rclcpp::shutdown(); return 0;
}
