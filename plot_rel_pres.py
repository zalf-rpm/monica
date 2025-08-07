import pandas as pd
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
import matplotlib.dates as mdates

# Load and prepare all datasets
df_suit = pd.read_csv("/home/alessandro/zalf-rpm/monica/crop_suitability.csv", header=0)
df_suit['Date'] = pd.to_datetime(df_suit['Date'])

df2 = pd.read_csv("/home/alessandro/zalf-rpm/monica/sim-min-out.csv", skiprows=[0, 2], header=0)
df2 = df2.iloc[:2540]
df2['Date'] = pd.to_datetime(df2['Date'], format='%Y-%m-%d', errors='coerce')
df2 = df2.dropna(subset=['Date'])

# Define your date range
start_date = "1991-09-01"
end_date = "1992-08-31"

# Filter both datasets to the same date range
df_suit = df_suit[(df_suit['Date'] >= start_date) & (df_suit['Date'] <= end_date)]
df2 = df2[(df2['Date'] >= start_date) & (df2['Date'] <= end_date)]

# Create complete date range for presence/suitability plots
all_dates = pd.date_range(start=start_date, end=end_date, freq='D')

# Convert numeric columns
numeric_cols = ['Yield1', 'Evapotranspiration1', 'AbBiom1', 'Yield2', 'Evapotranspiration2', 'AbBiom2', 'N']
for col in numeric_cols:
    df2[col] = pd.to_numeric(df2[col], errors='coerce')

# --- STYLE SETTINGS ---
weight_by_presence = True  # Set this to True/False as needed

# Presence/Suitability styles
presence_styles = {
    'WW1': {'color': 'grey', 'linestyle': '-', 'linewidth': 1, 'label': 'Crop 1'},
    'WW2': {'color': 'dimgray', 'linestyle': '--', 'linewidth': 1, 'label': 'Crop 2'}
}

suitability_colors = {
    'LightSuitability': 'gold',
    'TempSuitability': 'firebrick',
    'WaterSuitability': 'deepskyblue',
    'NitroSuitability': 'olivedrab',
    'OverallSuitability': 'purple'
}

# Crop variable styles
crop_colors = {
    'C1': 'grey',
    'C2': 'dimgray'
}

# Variables to plot from df2
crop_specific_vars = ['Stage', 'Yield', 'LAI', 'Height', 'EffRootDep']
general_vars = ['N', "Mois"]
num_plots = len(crop_specific_vars) + len(general_vars) + 2

# --- CREATE FIGURE ---
fig, axes = plt.subplots(3, 3, figsize=(20, 15), sharex=True)
axes = axes.flatten()

for i in range(num_plots, 9):
    axes[i].axis('off')

# --- PLOT 1: RELATIVE PRESENCE ---
ax_idx = 0
for crop_id in df_suit['CropID'].unique():
    crop_data = df_suit[df_suit['CropID'] == crop_id]
    complete_df = pd.DataFrame({'Date': all_dates}).merge(crop_data, on='Date', how='left')
    complete_df['RelativePresence'] = complete_df['RelativePresence'].fillna(0)

    style = presence_styles[crop_id].copy()
    label = style.pop('label')
    axes[ax_idx].plot(complete_df['Date'], complete_df['RelativePresence'],
                      label=label, **style)
axes[ax_idx].set_ylabel('Relative Presence', fontweight='bold')
axes[ax_idx].legend(loc='upper left', fontsize='small', framealpha=0.7)
axes[ax_idx].grid(True, alpha=0.3)

# --- PLOT 2: SUITABILITY METRICS ---
ax_idx = 1
for crop_id in df_suit['CropID'].unique():
    crop_data = df_suit[df_suit['CropID'] == crop_id]
    complete_df = pd.DataFrame({'Date': all_dates}).merge(crop_data, on='Date', how='left')

    for metric, color in suitability_colors.items():
        complete_df[metric] = complete_df[metric].fillna(0)
        linewidth = 2.5 if metric == 'OverallSuitability' else 1.5

        axes[ax_idx].plot(complete_df['Date'], complete_df[metric],
                          color=color,
                          linestyle=presence_styles[crop_id]['linestyle'],
                          linewidth=linewidth,
                          alpha=0.8 if metric == 'OverallSuitability' else 0.6)

axes[ax_idx].set_ylabel('Suitability Score', fontweight='bold')
axes[ax_idx].grid(True, alpha=0.3)

# Legends for second subplot
metric_legend_elements = [Line2D([0], [0], color=color, lw=2, label=metric)
                          for metric, color in suitability_colors.items()]
legend1 = axes[ax_idx].legend(handles=metric_legend_elements, loc='upper right',
                              fontsize='small', framealpha=0.7, title='Metrics')
axes[ax_idx].add_artist(legend1)

crop_legend_elements = [Line2D([0], [0], color='black', linestyle=style['linestyle'],
                               lw=2, label=style['label'])
                        for crop, style in presence_styles.items()]
axes[ax_idx].legend(handles=crop_legend_elements, loc='upper left',
                    fontsize='small', framealpha=0.7, title='Crops')

# --- PLOT 3+: CROP-SPECIFIC VARIABLES ---
for i, var in enumerate(crop_specific_vars, start=2):
    ax = axes[i]

    # Plot C1 data
    if f"{var}1" in df2.columns:
        if weight_by_presence and var not in ('Stage', 'Height', 'EffRootDep'):
            # Get C1 presence data and merge with df2 dates
            c1_presence = df_suit[df_suit['CropID'] == 'WW1'][['Date', 'RelativePresence']]
            plot_df = pd.merge(df2[['Date', f"{var}1"]], c1_presence, on='Date', how='left')
            plot_df['RelativePresence'] = plot_df['RelativePresence'].fillna(0)
            y_data = plot_df[f"{var}1"] * plot_df['RelativePresence']
            dates = plot_df['Date']
        else:
            y_data = df2[f"{var}1"]
            dates = df2['Date']

        ax.plot(dates, y_data,
                color=crop_colors['C1'],
                linestyle='-',
                label=f'{var} (Crop 1)' + (' [weighted]' if weight_by_presence and var not in ('Stage', 'Height', 'EffRootDep') else ''))

    # Plot C2 data
    if f"{var}2" in df2.columns:
        if weight_by_presence and var not in ('Stage', 'Height', 'EffRootDep'):
            # Get C2 presence data and merge with df2 dates
            c2_presence = df_suit[df_suit['CropID'] == 'WW2'][['Date', 'RelativePresence']]
            plot_df = pd.merge(df2[['Date', f"{var}2"]], c2_presence, on='Date', how='left')
            plot_df['RelativePresence'] = plot_df['RelativePresence'].fillna(0)
            y_data = plot_df[f"{var}2"] * plot_df['RelativePresence']
            dates = plot_df['Date']
        else:
            y_data = df2[f"{var}2"]
            dates = df2['Date']

        ax.plot(dates, y_data,
                color=crop_colors['C2'],
                linestyle='--',
                label=f'{var} (Crop 2)' + (' [weighted]' if weight_by_presence and var not in ('Stage', 'Height', 'EffRootDep') else ''))

    ax.set_ylabel(var, fontweight='bold')
    ax.legend(loc='upper left', fontsize='small', framealpha=0.7)
    ax.grid(True, alpha=0.3)
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%b-%Y'))

# --- PLOT N+: GENERAL VARIABLES ---
for j, var in enumerate(general_vars, start=2+len(crop_specific_vars)):
    ax = axes[j]

    if var in df2.columns:
        ax.plot(df2['Date'], df2[var],
                color='slategrey',
                linestyle='-',
                label=var)

    ax.set_ylabel(var, fontweight='bold')
    ax.legend(loc='upper left', fontsize='small', framealpha=0.7)
    ax.grid(True, alpha=0.3)
    ax.xaxis.set_major_formatter(mdates.DateFormatter('%b-%Y'))

plt.tight_layout()
plt.show()