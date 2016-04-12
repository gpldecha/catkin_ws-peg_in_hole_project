#include "peg_hole_policy/policies/state_machine.h"
#include <optitrack_rviz/type_conversion.h>

namespace ph_policy {

State_machine::State_machine(ros::NodeHandle& nh, Peg_sensor_model &peg_sensor_model)
    :peg_sensor_model(peg_sensor_model)
{
    state = AIR;
}

void State_machine::update(const arma::colvec3 &velocity,
                           const arma::colvec3& force,
                           const arma::colvec& belief_state_WF,
                           const arma::colvec& belief_state_SF,
                           const tf::Matrix3x3 &Rot,
                           const arma::colvec& Y_c,
                           const Get_back_on& get_back_on){
    states.clear();

    mode_SF(0) = belief_state_SF(0);
    mode_SF(1) = belief_state_SF(1);
    mode_SF(2) = belief_state_SF(2);

    mode_WF(0) = belief_state_WF(0);
    mode_WF(1) = belief_state_WF(1);
    mode_WF(2) = belief_state_WF(2);

    opti_rviz::type_conv::vec2tf(mode_WF,mode_tf);

    H = belief_state_WF(3);

    peg_sensor_model.update_model(mode_tf,Rot);
    peg_sensor_model.get_distance_features();
    dist_edge   = peg_sensor_model.get_distance_edge();

    STATES uncertainty;

    if(H > -4){
        uncertainty = HIGH_UNCERTAINTY;
    }else if(H > -8){
        uncertainty = MEDIUM_UNCERTAINTY;
    }else{
        uncertainty = LOW_UNCERTAINTY;
    }
    states.push_back(uncertainty);

    if(Y_c(0) == 0)
    {
        states.push_back(AIR);
    }else if(Y_c(0) == 1){
        states.push_back(TABLE);
    }

    if(Y_c(1) == 1){
        states.push_back(EDGE);
    }


    ROS_INFO_STREAM_THROTTLE(1.0,"dist_edge: " <<peg_sensor_model.get_distance_edge() << " H: " <<  H);

    if(peg_sensor_model.get_distance_edge() < 0.04 && (H < -7.0) ){
        states.push_back(CLOSE_EDGE);
    }

    if(arma::norm(mode_SF) < 0.08 && (H < -8.0)){
        states.push_back(CLOSE_SOCKET);
        if(Y_c(1) == 1){
            states.push_back(SOCKET);
        }
    }

    if( (std::fabs(mode_SF(1)) < 0.04 && std::fabs(mode_SF(2)) < 0.04) && std::fabs(mode_SF(0) < 0.05))
    {
        states.push_back(MODE_CLOSE_HOLE);
    }

    if(is_off_table(mode_SF,force,uncertainty) || get_back_on.get_status() == Get_back_on::STATUS::RUNNING)
    {
        states.push_back(OFF_TABLE);
    }
    if(std::fabs(force(1)) >= 1.0 || std::fabs(force(0)) >= 1.0){
        states.push_back(STUCK_EDGE);
    }
    if(mode_SF(0) <= 0.03 && std::fabs(mode_SF(1)) < 0.03 && mode_SF(2) > 0.03 &&  mode_SF(2) < 0.06){
        states.push_back(TOP_SOCKET);
    }

    if(mode_SF(0) <= 0.03 && std::fabs(mode_SF(1)) < 0.03 && (mode_SF(2) < -0.03) &&  mode_SF(2) > -0.06){
        states.push_back(BOTTOM_SOCKET);
    }

    if( (std::fabs(mode_SF(1)) < 0.005 && std::fabs(mode_SF(2)) < 0.005) && has_state(LOW_UNCERTAINTY,states) )
    {
        states.push_back(SOCKET_ENTRY);
    }

    if( (std::fabs(mode_SF(1)) < 0.005 && std::fabs(mode_SF(2)) < 0.005) && std::fabs(mode_SF(0) < 0.015) && has_state(LOW_UNCERTAINTY,states) )
    {
        states.push_back(SLIGHTLY_IN);
    }

}

bool State_machine::is_off_table(const arma::colvec3 &mode_SF, const arma::colvec3 &force,STATES uncertainty){

    // when belived loaction (x is negative by some value)
    if(mode_SF(0) < 0.015 && arma::norm(force) <= 0.1 && (arma::norm(mode_SF) > 0.1 && (uncertainty != STATES::LOW_UNCERTAINTY)) )
    {
        return true;
    }else{
        return false;
    }

}

const void State_machine::print(double seconds) const{
    std::string msg = "[";
    for(std::size_t i = 0; i < states.size()-1;i++){
        msg = msg + states2str(states[i]) + "] [";
    }
    msg = msg +  states2str(states[states.size()-1]) + "]";

    if(seconds==0){
        ROS_INFO_STREAM("states: " << msg);
    }else{
        ROS_INFO_STREAM_THROTTLE(seconds,"states: " << msg);
    }

}

const std::vector<STATES> &State_machine::get_state() const{
    return states;
}

}
