#include <ctcr_model.h>


CTCRModel::CTCRModel(std::array<double,3> length, std::array<double,3> ro, std::array<double,3> ri, std::array<double,3> straight_length, std::array<double,3> curvature, double youngs_modulus, int pts_per_seg, Eigen::Matrix4d base_frame) {
	
	m_length[0]         = length[0];
    m_length[1]         = length[1];
    m_length[2]         = length[2];
    
	m_ro[0]         = ro[0];
    m_ro[1]         = ro[1];
    m_ro[2]         = ro[2];
    
	m_ri[0]         = ri[0];
    m_ri[1]         = ri[1];
    m_ri[2]         = ri[2];
    
	m_straight_length[0]  = straight_length[0];
    m_straight_length[1]  = straight_length[1];
    m_straight_length[2]  = straight_length[2];
    
	m_curvature[0]      = curvature[0];
    m_curvature[1]      = curvature[1];
    m_curvature[2]      = curvature[2];
    
	m_youngs_modulus    = youngs_modulus;
	m_points_per_seg	= pts_per_seg;
    m_base_frame		= base_frame;
    
    
    m_current_config.resize(6,1);
    m_current_config.setZero();
}

CTCRModel::~CTCRModel() {

}

// This function should implement the forward kinematics of a concnetric tube continuum robot (i.e. mapping tube rotations and translations in joint space to the robot's end-effector and shape)
// Inputs:
// q						6x1 matrix/vector holding actuation values.
//							The first three entries are each tube's rotation, while the last three are the tube's translations.
//
// Outputs:
// ee_frame					4x4 matrix storing the end-effector frame resulting from the actuation q.
// backbone_centerline		4x4(m*n+1) dimensional matrix storing n frames for each of the m subsegments of the CTCR.
//							The leftmost 4x4 block should store the initial base/disk frame of the robot.
// tube_ind					Vector with m entries, specifying the outermost tube for each of the m subsegments.
// boolean return value		True if kinematics have been calculated successfully, false if not.
//							Also return false, if the joints limits and inequality constraints are invalidated.
bool CTCRModel::forward_kinematics(Eigen::Matrix4d &ee_frame, Eigen::MatrixXd &backbone_centerline, std::vector<int> &tube_ind, Eigen::MatrixXd q)
{


    //YOUR CODE GOES HERE


    double beta1 = q(3);
    double beta2 = q(4);
    double beta3 = q(5);
    double L1 = m_length[0];
    double L2 = m_length[1];
    double L3 = m_length[2];
    double t1 = beta1 + L1;
    double t2 = beta2 + L2;
    double t3 = beta3 + L3;

    // Check two constrains
    if (!(beta1 <= beta2 && beta2 <= beta3 && beta3 <= 0) or !(t3 <= t2 && t2 <= t1)) {

        return false;
    }



    // Calculate the transition points
    std::vector<double> T;
    std::vector<double> T_straight_lst;
    std::vector<double> T_total_lst;
    for (int t = 0; t < 3; ++t) {
        double T_straight = q(3 + t) + m_straight_length[t];
        double T_total = q(3 + t) + m_length[t];
        // Add non-zero to the vector. Set threshold to avoid FP overflow issues
        T_straight_lst.push_back(T_straight);
        T_total_lst.push_back(T_total);
        if (T_straight >= 0) {
            T.push_back(T_straight);
        }
        if (T_total >= 0) {
            T.push_back(T_total);
        }
    }

    // Sort the vector and remove the duplicates
    std::sort(T.begin(), T.end());
    T.erase(unique(T.begin(), T.end()), T.end());


    // Arc Parameters per Segment
    std::vector<double> kappa_lst;
    std::vector<double> phi_lst;
    std::vector<double> phi_lst_corrected;
    std::vector<double> l_lst;

    for (int i = 0; i < T.size(); ++i) {
        double kappa_i, phi_i, l_i;

        // Determine tubes that present in segment i
        bool curved = false;
        int outer_tube = 0;
        double denominator = 0.0;
        Eigen::Matrix<double, 2, 1> numerator;
        numerator.setZero();
        double I_j;
        // From inner tube to outer tube
        for (int j = 0; j < 3; ++j) {
            // Check if the current transition point is on the current tube or not.
            if (T[i] <= T_total_lst[j]) {
                ++outer_tube;
                I_j = M_PI /64.0 *(pow(m_ro[j], 4) - pow(m_ri[j], 4));
                denominator += m_youngs_modulus * I_j;

                // if the current transition point is on the curved part of the tube?
                if (T[i] > T_straight_lst[j]) {
                    curved = true;
                    Eigen::Matrix<double, 2, 1> alpha_vec;
                    alpha_vec.setZero();
                    alpha_vec << std::cos(q(j)), std::sin(q(j));
                    numerator += m_youngs_modulus * I_j * m_curvature[j] * alpha_vec;
                }
            }
        }

        // if the transition point is on curved part of any 3 tubes
        Eigen::Matrix<double, 2, 1> kappa_xy = numerator / denominator;
        double kappa_x = kappa_xy(0);
        double kappa_y = kappa_xy(1);
        kappa_i = sqrt(pow(kappa_x, 2) + pow(kappa_y, 2));
        phi_i = std::atan2(kappa_y, kappa_x);


        l_i = (i == 0) ? T[i] : T[i] - T[i - 1];
        kappa_lst.push_back(kappa_i);
        phi_lst.push_back(phi_i);
        l_lst.push_back(l_i);
        tube_ind.push_back(outer_tube);
    }

    // Phi correction
    for(int i = 0; i < T.size(); ++i)
    {
        double phi_i_corrected = (i == 0)? phi_lst[0] : phi_lst[i] - phi_lst[i-1];
        phi_lst_corrected.push_back(phi_i_corrected);
    }



    // Get the disk frames and end-effector frame
    backbone_centerline = arc_to_x(m_base_frame, kappa_lst, l_lst, phi_lst_corrected, m_points_per_seg, false);
    ee_frame = backbone_centerline.block(0, backbone_centerline.cols() - 4, 4, 4);




    //YOUR CODE ENDS HERE


    //Setting the member variables accordingly
    m_ee_frame = ee_frame;
    m_backbone_centerline = backbone_centerline;
    m_current_config = q;

    return true;

}


// This function should calculate and return the body Jacobian of a concnetric tube continuum robot using a simple finite differences approach
// Inputs:
// q						6x1 matrix/vector holding actuation values.
//							The first three entries are each tube's rotation, while the last three are the tube's translations.
// J						6x6 body Jacobian matrix, relating joint space velocities with end-effector twist
//							The first three rows correspond to the rotational part of a twist, while the last three rows correspond to the translational part
//
// Outputs:
// boolean return value		Return false, if the joints limits and inequality constraints are invalidated for the requested values in q.
//							Return true otherwise (and proceed to calculate and return J).
bool CTCRModel::get_body_jacobian(Eigen::MatrixXd &J, Eigen::MatrixXd q)
{
	//Resize J and set to zero
	J.setZero(6,6);
	
	//Evaluate at current configuration q
	Eigen::Matrix4d init_ee_frame;
	Eigen::MatrixXd init_backbone_centerline;
	std::vector<int> tube_ind;
	Eigen::MatrixXd init_q = q;
	
	if(!forward_kinematics(init_ee_frame, init_backbone_centerline, tube_ind, q))
	{
		//Return false if joint value constraints are not met
		std::cout << "forward false" << std::endl;
		return false;
	}
	//Calculate the Body Jacobian using Finite Differences here (YOUR CODE GOES HERE)
	for(int i = 0; i < 6; ++i)
    {
        Eigen::MatrixXd q_ic = q;
        q_ic(i, 0) += 1e-4;
        Eigen::Matrix4d init_ee_frame_c;
        Eigen::MatrixXd init_backbone_centerline_c;
        std::vector<int> tube_ind_c;
        forward_kinematics(init_ee_frame_c, init_backbone_centerline_c, tube_ind_c, q_ic);
        Eigen::Matrix<double, 6, 1> delta_X_h = calculate_desired_body_twist(init_ee_frame_c, init_ee_frame) / 1e-4;
        J.block<6, 1>(0, i) << delta_X_h;

    }


	//YOUR CODE ENDS HERE

	
	//Setting the member variables accordingly if q was valid
	m_ee_frame = init_ee_frame;
	m_backbone_centerline = init_backbone_centerline;
	m_current_config = init_q;
	
	return true;
}

Eigen::MatrixXd CTCRModel::get_current_config()
{
	return m_current_config;
}

Eigen::Matrix4d CTCRModel::get_ee_frame()
{
	return m_ee_frame;
}

Eigen::MatrixXd CTCRModel::get_backbone_centerline()
{
	return m_backbone_centerline;
}

Eigen::Matrix4d CTCRModel::get_base_frame()
{
	return m_base_frame;
}

