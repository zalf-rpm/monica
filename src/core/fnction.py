
    # Extract required variables from JSON
    LAI_A, LAI_B = spA_crop["leafAreaIndex"], spB_crop["leafAreaIndex"]
    Height_A, Height_B = spA_crop["cropHeight"], spB_crop["cropHeight"]
    Height_A_max, Height_B_max = spA_crop["pcMaxCropHeight"], spB_crop["pcMaxCropHeight"]
    # Define max LAI (could be dynamic or user-defined)
    LAI_max = {"spA": 7, "spB": 10}
    # Compute communal properties
    LAI_comm = (LAI_A * spA_rel_pres) + (LAI_B * spB_rel_pres)
    Height_comm = (Height_A * spA_rel_pres) + (Height_B * spB_rel_pres)
    # Compute suitability factors
    f_r_A = 0.5 + ((LAI_max["spA"] - LAI_comm) / (LAI_max["spA"] + LAI_comm) +
                   2 * (Height_A_max - Height_comm) / (Height_A_max + Height_comm)) / 6
    f_r_B = 0.5 + ((LAI_max["spB"] - LAI_comm) / (LAI_max["spB"] + LAI_comm) +
                   2 * (Height_B_max - Height_comm) / (Height_B_max + Height_comm)) / 6


    def triangular_temperature(T_a, T_b, T_opt, T_coff):
        """
        Compute the temperature suitability function f_T based on a triangular approach.
        Parameters:
        T_a (float): Mean air temperature (°C)
        T_b (float): Minimum temperature for photosynthesis (°C)
        T_opt (float): Optimum temperature for photosynthesis (°C)
        T_coff (float): Cutoff (maximum) temperature for photosynthesis (°C)
        Returns:
        float: Suitability index (0 to 1)
        """
        if T_b < T_a <= T_opt:
            return (T_a - T_b) / (T_opt - T_b)
        elif T_opt < T_a <= T_coff:
            return 1 - ((T_a - T_opt) / (T_coff - T_opt))
        else:
            return 0  # Outside the suitable temperature range


    def suitability_water(spA_file, spB_file, spA_rel_pres, spB_rel_pres):
        """
        Compute suitability factors for both species based on soil water content (SWC) and rooting depth.
        """
        # Read JSON files
        with open(spA_file, "r") as file:
            spA_data = json.load(file)
        with open(spB_file, "r") as file:
            spB_data = json.load(file)
        # Extract variables for species A
        spA_soil = spA_data["modelState"]["soilColumn"]
        spA_crop = spA_data["modelState"]["currentCropModule"]
        spB_soil = spB_data["modelState"]["soilColumn"]
        spB_crop = spB_data["modelState"]["currentCropModule"]
        root_layer_A = spA_crop["rootingDepth"]
        root_layer_B = spB_crop["rootingDepth"]
        SWC_A = np.mean([layer["soilMoistureM3"] for layer in spA_soil["layers"][:root_layer_A]])
        SWC_B = np.mean([layer["soilMoistureM3"] for layer in spB_soil["layers"][:root_layer_B]])
        DT_A = np.mean(spA_crop["pcDroughtStressThreshold"])  # Drought tolerance (0-1)
        DT_B = np.mean(spB_crop["pcDroughtStressThreshold"])
        RD_A = spA_crop["rootingDepthM"]  # Current rooting depth (cm)
        RD_max_A = spA_crop["pcCropSpecificMaxRootingDepth"]  # Max rooting depth (cm)
        RD_B = spB_crop["rootingDepthM"]
        RD_max_B = spB_crop["pcCropSpecificMaxRootingDepth"]
        SWC_comm = SWC_A*spA_rel_pres+ SWC_B*spB_rel_pres
        RD_comm = RD_A*spA_rel_pres+ RD_B*spB_rel_pres
        # Calculate f_PAW for species A
        if SWC_comm < 1 - DT_A:
            f_PAW_A = 0
        else:
            f_PAW_A = (DT_A - 1) / DT_A + SWC_comm / DT_A
        # Calculate f_roots for species A
        f_roots_A = 1 - (RD_comm / RD_max_A) if RD_comm <= RD_max_A else 0
        # Calculate f_PAW for species B
        if SWC_comm < 1 - DT_B:
            f_PAW_B = 0
        else:
            f_PAW_B = (DT_B - 1) / DT_B + SWC_comm / DT_B
        # Calculate f_roots for species B
        f_roots_B = 1 - (RD_comm / RD_max_B) if RD_comm <= RD_max_B else 0
        # Calculate final water suitability index
        f_water_A = (f_PAW_A + f_roots_A) / 2
        f_water_B = (f_PAW_B + f_roots_B) / 2
        return float(f_water_A), float(f_water_B)


    def calculate_HSf(Sfi_values):
        """
        Compute hierarchy-corrected suitability (HSf) for a given list of stressor values.
        Parameters:
            Sfi_values (list of float): List of suitability values (stressors) for different hierarchy levels
        Returns:
            list of float: Hierarchy-corrected suitability values (HSf).
        """
    HSf_values = []
    for q in range(len(Sfi_values)):
        if q == 0:  # q = 1
            HSf_q = Sfi_values[q]
        elif q == 1:  # q = 2
            HSf_q = Sfi_values[q] * np.sqrt(Sfi_values[q - 1])
        else:  # q > 2
            HSf_q = np.sqrt(HSf_values[q - 1]) * np.sqrt(Sfi_values[q - 1]) * Sfi_values[q]
        HSf_values.append(float(HSf_q))
    return HSf_values