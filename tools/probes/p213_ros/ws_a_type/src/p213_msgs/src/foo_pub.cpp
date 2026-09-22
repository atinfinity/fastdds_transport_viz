#include <chrono>
#include <rclcpp/rclcpp.hpp>
#include <p213_msgs/msg/foo.hpp>
int main(int argc, char ** argv) {
  rclcpp::init(argc, argv);
  auto n = rclcpp::Node::make_shared("foo_pub");
  auto p = n->create_publisher<p213_msgs::msg::Foo>("/p213", 10);
  int i = 0;
  auto t = n->create_wall_timer(std::chrono::milliseconds(500), [&]() {
    p213_msgs::msg::Foo m; m.s = "cpp_pub_" + std::to_string(i++); p->publish(m);
    RCLCPP_INFO(n->get_logger(), "pub %s matched=%zu", m.s.c_str(), p->get_subscription_count()); });
  rclcpp::spin(n); rclcpp::shutdown(); return 0;
}
