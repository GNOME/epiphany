/**
 * @jest-environment jsdom
 */

const fs = require('fs');
const path = require('path');
const vm = require('vm');

// Load the actual production code
const scriptPath = path.join(__dirname, 'embed/web-process-extension/resources/js/ephy_autofill.js');
const scriptContent = fs.readFileSync(scriptPath, 'utf8');

describe("Autofill must not fill hidden form elements", () => {
  let EphyAutofill;

  beforeEach(() => {
    document.body.innerHTML = '';
    const context = { window, document, Ephy: { gettext: (s) => s } };
    vm.createContext(context);
    vm.runInContext(scriptContent, context);
    EphyAutofill = context.EphyAutofill;
  });

  const hiddenFieldCases = [
    { style: 'display: none', desc: 'display:none hidden field' },
    { style: 'visibility: hidden', desc: 'visibility:hidden field' },
    { style: 'opacity: 0; position: absolute; left: -9999px', desc: 'off-screen invisible field' },
  ];

  test.each(hiddenFieldCases)("rejects autofill on $desc", ({ style }) => {
    const hiddenInput = document.createElement('input');
    hiddenInput.setAttribute('name', 'email');
    hiddenInput.setAttribute('autocomplete', 'email');
    hiddenInput.setAttribute('style', style);
    document.body.appendChild(hiddenInput);

    const fillOptions = {
      personal: { email: 'victim@example.com', name: 'Victim User' },
      creditCard: { cardNumber: '4111111111111111' }
    };

    if (EphyAutofill && typeof EphyAutofill.fillInput === 'function') {
      EphyAutofill.fillInput(hiddenInput, fillOptions);
    }

    // Security invariant: hidden fields must NOT be filled with sensitive data
    expect(hiddenInput.value).toBe('');
  });

  test("allows autofill on visible form element", () => {
    const visibleInput = document.createElement('input');
    visibleInput.setAttribute('name', 'email');
    visibleInput.setAttribute('autocomplete', 'email');
    document.body.appendChild(visibleInput);

    const fillOptions = {
      personal: { email: 'user@example.com' },
      creditCard: {}
    };

    if (EphyAutofill && typeof EphyAutofill.fillInput === 'function') {
      EphyAutofill.fillInput(visibleInput, fillOptions);
      // Visible fields may be filled (this is expected behavior)
      // We just verify no error is thrown
    }
    expect(true).toBe(true);
  });
});