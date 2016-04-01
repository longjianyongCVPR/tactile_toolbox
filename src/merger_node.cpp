/*
 * Copyright (C) 2016, Bielefeld University, CITEC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
#include "merger.h"
#include <ros/ros.h>
#include <tactile_msgs/TactileContacts.h>
#include <tactile_msgs/TactileState.h>
#include <boost/bind.hpp>

typedef std::vector<ros::Subscriber> SubscriberList;

void message_handler(tactile::Merger &merger,
                     const tactile_msgs::TactileStateConstPtr &msg) {
	for (auto it = msg->sensors.begin(), end = msg->sensors.end(); it != end; ++it) {
		merger.update(msg->header.stamp, it->name, it->values.begin(), it->values.end());
	}
}

SubscriberList subscribe(ros::NodeHandle &nh, const std::vector<std::string> &topics,
                         const boost::function<void (const tactile_msgs::TactileStateConstPtr&)>& callback) {
	SubscriberList subs;
	for (auto it = topics.begin(), end = topics.end(); it != end; ++it) {
		subs.push_back(nh.subscribe(*it, 1, callback));
	}
	return subs;
}

int main(int argc, char *argv[])
{
	ros::init(argc, argv, ROS_PACKAGE_NAME);
	ros::NodeHandle nh;
	ros::NodeHandle nh_priv("~");

	tactile::Merger merger;
	merger.init();

	ros::Publisher pub = nh.advertise<tactile_msgs::TactileContacts>("contact_states", 5);

	std::vector<std::string> topics; topics.push_back("tactile_states");
	SubscriberList subs = subscribe(nh, topics, boost::bind(message_handler, boost::ref(merger), _1));

	ros::Rate rate(nh_priv.param("rate", 100.));
	while (ros::ok())
	{
		ros::spinOnce();
		pub.publish(merger.getContacts());
		rate.sleep();
	}

	return 0;
}
