import {
  HashRouter as Router,
  Routes,
  Route,
  NavLink
} from "react-router-dom";
import './App.css';

// Imports bootstrap css after overriding some variables
import './custom.scss'

import * as Icon from 'react-bootstrap-icons';

import Dashboard from "./Dashboard.js";
import Log from "./Log.js";

// Dependency used by Bootstrap
import '@popperjs/core/dist/cjs/popper-lite'

// Only import the Bootstrap components (JS part) that we actually need
// see https://getbootstrap.com/docs/5.0/customize/optimize/

// import 'bootstrap/js/dist/alert';
import 'bootstrap/js/dist/button';
// import 'bootstrap/js/dist/carousel';
//import 'bootstrap/js/dist/collapse';
// import 'bootstrap/js/dist/dropdown';
// import 'bootstrap/js/dist/modal';
// import 'bootstrap/js/dist/popover';
// import 'bootstrap/js/dist/scrollspy';
import 'bootstrap/js/dist/tab';
//import 'bootstrap/js/dist/toast';
// import 'bootstrap/js/dist/tooltip';

export default function App() {
  // Have a global, optional URL "prefix" so one can have the App
  // served by localhost during development but talk to the API of
  // a REAL dongle
  window.urlPrefix = '';
  window.urlPrefix = 'http://169.254.31.1'; // comment line if not used; NEVER COMMIT

  return (
    <Router>
      <div>
        <nav className="navbar navbar-expand-sm">
          { false && <span className="navbar-brand"><img src="media/logo-fts.svg" alt="logo-fts" width={69} height={39} style={{ marginLeft: "20px" }} className="filterFgColor"></img></span> }
          <ul className="navbar-nav nav-tabs nav-fill mr-auto">
            <li className="nav-item active">
              <NavLink to="/" data-toggle="tab" className="nav-link"><Icon.Gear style={{ marginRight: '8px' }}/>Dashboard</NavLink>
            </li>
            <li className="nav-item">
              <NavLink to="/log" data-toggle="tab" className="nav-link">Log</NavLink>
            </li>
          </ul>
        </nav>

        {/* A <Routes> looks through its children <Route>s and
            renders the first one that matches the current URL. */}
        <Routes>
          <Route path="/log/*" element={<Log />} />
          <Route path="/" element={<Dashboard />} />
        </Routes>
      </div>
    </Router>
  );
}
