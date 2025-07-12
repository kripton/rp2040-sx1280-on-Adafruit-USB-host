import React from 'react';
import moment from "moment";

class Dashboard extends React.Component {
    constructor() {
        super();
        this.state = {
            meters: {
                remote: {}
            },
            loading: false,
            selectedTab: '',
            lastUpdate: '',
        };
    }


    componentDidMount() {
        this.updateMeters.bind(this)();
    }

    updateMeters() {
        // Check if there is already a request running. If so, do nothing
        if (this.state.loading) {
            return;
        }

        this.setState({ loading: true });
        const url = window.urlPrefix + '/api/meters.json';
        fetch(url)
            .then(res => res.json())
            .catch(
                () => { this.setState({ loading: false }); this.updateMeters(); }
            )
            .then(
                (result) => {
                    if (result) {
                        console.log('Meters fetched: ', result);
                        this.setState({ loading: false, meters: result, lastUpdate: moment().format('YYYY-MM-DDTHH:mm:ss') });
                    }
                }
            ).finally(
                async () => {
                    this.setState({ loading: false });
                    await new Promise(resolve => setTimeout(resolve, 200));
                    this.updateMeters();
                }
            );
    }

    regToFloat(meter, regId) {
        if (meter[regId] && meter[regId].val) {
            let regVal = meter[regId].val;
            let uintArray = new Uint8Array(regVal.match(/[\da-f]{2}/gi).map(function (h) { return parseInt(h, 16) }));
            let dv = new DataView(uintArray.buffer);
            return dv.getFloat32().toFixed(2);
        } else {
            return '-.--';
        }
    }

    getHighestTimeStamp(meter) {
        let highestTimeStamp = 0;
        for (const regId in meter) {
            if (meter[regId].ts && meter[regId].ts > highestTimeStamp) {
                highestTimeStamp = meter[regId].ts;
            }
        }
        return highestTimeStamp;
    }

    render() {
        return (
            <>
                <ul className="nav nav-tabs">
                    <li key="overview" className="nav-item" onClick={() => { this.setState({ selectedTab: '' }); }}>
                        <a className={this.state.selectedTab == '' ? "nav-link active" : "nav-link"} aria-current="page" href="#">Overview.</a>
                    </li>
                    {(() => {
                        let meters = [];
                        if (Object.entries(this.state.meters.remote).length > 0) {
                        }
                        for (const meterId in this.state.meters.remote) {
                            const meter = this.state.meters.remote[meterId];
                            console.log(`${meterId}: ${meter}`);
                            meters.push(
                                <li key={"tab_" + meterId} className="nav-item" onClick={() => { this.setState({ selectedTab: meterId }); }}>
                                    <a className={this.state.selectedTab == meterId ? "nav-link active" : "nav-link"} href="#">{meterId} (R)</a>
                                </li>
                            );
                        }
                        return meters;
                    })()}
                </ul>
                {this.state.selectedTab == '' ?
                    <>
                        <div className="card">
                            <div className="card-body">
                                <h5 className="card-title">Overview</h5>
                                <p className="card-text">This is the overview of the dashboard.<br/>Last update: {this.state.lastUpdate}</p>
                            </div>
                        </div>
                        {(() => {
                            let meters = [];
                            for (const meterId in this.state.meters.remote) {

                                // A meter is only considrered as a meter if the following register values are known:
                                // U1, U2, U3, I1, I2, I3, P1, P2, P3, Pf1, Pf2, Pf3, f
                                if (!this.state.meters.remote[meterId].hasOwnProperty("0000") ||
                                    !this.state.meters.remote[meterId].hasOwnProperty("0002") ||
                                    !this.state.meters.remote[meterId].hasOwnProperty("0004") ||
                                    !this.state.meters.remote[meterId].hasOwnProperty("0006") ||
                                    !this.state.meters.remote[meterId].hasOwnProperty("0008") ||
                                    !this.state.meters.remote[meterId].hasOwnProperty("000a") ||
                                    !this.state.meters.remote[meterId].hasOwnProperty("000c") ||
                                    !this.state.meters.remote[meterId].hasOwnProperty("000e") ||
                                    !this.state.meters.remote[meterId].hasOwnProperty("0010") ||
                                    !this.state.meters.remote[meterId].hasOwnProperty("001e") ||
                                    !this.state.meters.remote[meterId].hasOwnProperty("0020") ||
                                    !this.state.meters.remote[meterId].hasOwnProperty("0022") ||
                                    !this.state.meters.remote[meterId].hasOwnProperty("0046") ||
                                    !this.state.meters.remote[meterId].hasOwnProperty("00e0")) {
                                    console.warn(`Meter ${meterId} does not have all required registers. Skipping.`);
                                    continue;
                                }

                                const meter = this.state.meters.remote[meterId];
                                const highestTimeStamp = this.getHighestTimeStamp(meter); // In "ms since boot" of receiver
                                const bootTime = new Date(Date.now() - this.state.meters.tsNow);
                                let lastUpdate = new Date(bootTime.valueOf() + highestTimeStamp);
                                let luDiffMs = this.state.meters.tsNow -highestTimeStamp;
                                if (luDiffMs < 0) {
                                    luDiffMs = 1;
                                    lastUpdate = new Date(Date.now());
                                }
                                //const dur = moment.duration(luDiffMs, 'milliseconds').humanize();
                                meters.push(
                                    <div key={"card_" + meterId} className="card">
                                        <div className="card-body">
                                            <h5 className="card-title">Meter {meterId}<br/>RSSI: {meter.rssi}<br/>Last updated: {moment(lastUpdate).fromNow()} ({(luDiffMs / 1000).toFixed(0)}s) ({highestTimeStamp})</h5>
                                            <table className="card-text"><tbody>
                                                <tr>
                                                    <td><b>U1:</b></td><td align='right'>{this.regToFloat(meter, "0000")}V</td>
                                                    <td>&nbsp;&nbsp;</td>
                                                    <td><b>U2:</b></td><td align='right'>{this.regToFloat(meter, "0002")}V</td>
                                                    <td>&nbsp;&nbsp;</td>
                                                    <td><b>U3</b></td><td align='right'>{this.regToFloat(meter, "0004")}V</td>
                                                </tr>
                                                <tr>
                                                    <td><b>I1:</b></td><td align='right'>{this.regToFloat(meter, "0006")}A</td>
                                                    <td>&nbsp;&nbsp;</td>
                                                    <td><b>I2:</b></td><td align='right'>{this.regToFloat(meter, "0008")}A</td>
                                                    <td>&nbsp;&nbsp;</td>
                                                    <td><b>I3:</b></td><td align='right'>{this.regToFloat(meter, "000a")}A</td>
                                                </tr>
                                                <tr>
                                                    <td><b>P1:</b></td><td align='right'>{this.regToFloat(meter, "000c")}W</td>
                                                    <td>&nbsp;&nbsp;</td>
                                                    <td><b>P2:</b></td><td align='right'>{this.regToFloat(meter, "000e")}W</td>
                                                    <td>&nbsp;&nbsp;</td>
                                                    <td><b>P3:</b></td><td align='right'>{this.regToFloat(meter, "0010")}W</td>
                                                </tr>
                                                <tr>
                                                    <td><b>Pf1:</b></td><td align='right'>{this.regToFloat(meter, "001e")}</td>
                                                    <td>&nbsp;&nbsp;</td>
                                                    <td><b>Pf2:</b></td><td align='right'>{this.regToFloat(meter, "0020")}</td>
                                                    <td>&nbsp;&nbsp;</td>
                                                    <td><b>Pf3:</b></td><td align='right'>{this.regToFloat(meter, "0022")}</td>
                                                </tr>
                                                <tr><td>&nbsp;</td></tr>
                                                <tr>
                                                    <td><b>f:</b></td><td align='right'>{this.regToFloat(meter, "0046")}Hz</td>
                                                    <td>&nbsp;&nbsp;</td>
                                                    <td><b>I<sub>N</sub>:</b></td><td align='right'>{this.regToFloat(meter, "00e0")}A</td>
                                                    <td>&nbsp;&nbsp;</td>
                                                    <td><b>E<sub>ges</sub>:</b></td><td align='right'>{this.regToFloat(meter, "0180")}kWh</td>
                                                </tr>
                                            </tbody></table>
                                        </div>
                                    </div>
                                );
                            }
                            return meters;
                        })()}
                    </> :
                    <>
                        <div className="card">
                            <div className="card-body">
                                <h5 className="card-title">Meter: {this.state.selectedTab}</h5>
                                <p className="card-text">Details for meter {this.state.selectedTab}.</p>
                            </div>
                        </div>
                    </>
                }
            </>
        );
    }
}
export default Dashboard;
